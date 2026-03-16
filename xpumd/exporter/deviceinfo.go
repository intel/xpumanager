//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"context"
	"sync"

	pb "github.com/intel/xpumanager/xpumd/exporter/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/xpumd/exporter/internal/metadata"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
)

type deviceInfoServer struct {
	pb.UnimplementedDeviceInfoServer
	logger    *zap.SugaredLogger
	telemetry *metadata.TelemetryBuilder

	stopChan chan struct{}

	// Cached health data
	healthDataLock sync.RWMutex
	healthData     *pb.DeviceHealthResponse
	// Active clients
	deviceHealthClients sync.Map
}

func newDeviceInfoServer(logger *zap.SugaredLogger, tb *metadata.TelemetryBuilder) *deviceInfoServer {
	return &deviceInfoServer{
		logger:    logger,
		telemetry: tb,
		stopChan:  make(chan struct{}),
	}
}

func (s *deviceInfoServer) WatchDeviceHealth(req *pb.WatchDeviceHealthRequest, stream pb.DeviceInfo_WatchDeviceHealthServer) error {
	s.logger.Info("new DeviceHealth client connected")
	if err := s.sendHealthData(stream); err != nil {
		// Bail out if the initial send sync fails
		return err
	}

	err := make(chan error)
	s.deviceHealthClients.Store(stream, err)
	defer s.deviceHealthClients.Delete(stream)

	select {
	case <-s.stopChan:
		s.logger.Info("server stopping, closing DeviceHealth stream")
		return nil
	case <-stream.Context().Done():
		s.logger.Infow("stream closed", "error", stream.Context().Err())
		return stream.Context().Err()
	case e := <-err:
		s.logger.Infow("stopping DeviceHealth stream", "error", e)
		return e
	}
}

func (s *deviceInfoServer) stop() {
	close(s.stopChan)
}

func (s *deviceInfoServer) updateHealthData(md pmetric.Metrics) {
	s.healthDataLock.Lock()
	defer s.healthDataLock.Unlock()

	mt := newMetricsTranslator(s.logger)
	s.healthData = mt.translate(md)

	s.broadcastHealthData()
}

func (s *deviceInfoServer) broadcastHealthData() {
	s.deviceHealthClients.Range(func(key, value any) bool {
		stream, ok := key.(pb.DeviceInfo_WatchDeviceHealthServer)
		if !ok {
			panic("invalid type assertion for device health client stream")
		}
		errChan, ok := value.(chan error)
		if !ok {
			panic("invalid type assertion for device health client stop channel")
		}
		if err := s.sendHealthData(stream); err != nil {
			errChan <- err
		}
		return true
	})
}

// sendHealthData sends the cached health data to the given stream. Expects
// the caller to hold the appropriate locks.
func (s *deviceInfoServer) sendHealthData(stream pb.DeviceInfo_WatchDeviceHealthServer) error {
	s.logger.Info("sending device health data")
	if err := stream.Send(s.healthData); err != nil {
		s.logger.Errorw("failed to send health data to client", "error", err)
		s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
		return err
	}
	s.telemetry.ExporterRequestsSent.Add(context.Background(), 1)
	return nil
}
