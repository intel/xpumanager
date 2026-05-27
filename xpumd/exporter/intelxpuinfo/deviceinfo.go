//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"sync"

	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/internal/metadata"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
)

type deviceInfoServer struct {
	pb.UnimplementedDeviceInfoServer
	logger    *zap.SugaredLogger
	telemetry *metadata.TelemetryBuilder
	cfg       *Config

	stopChan chan struct{}

	// Cached health data
	healthDataLock sync.RWMutex
	healthData     *pb.DeviceHealthResponse
	// Active health clients
	deviceHealthClients sync.Map
	// Active event clients
	deviceEventClients sync.Map
}

func newDeviceInfoServer(logger *zap.SugaredLogger, tb *metadata.TelemetryBuilder, cfg *Config) *deviceInfoServer {
	return &deviceInfoServer{
		logger:    logger,
		telemetry: tb,
		cfg:       cfg,
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

func (s *deviceInfoServer) WatchDeviceEvents(req *pb.WatchDeviceEventsRequest, stream pb.DeviceInfo_WatchDeviceEventsServer) error {
	s.logger.Info("new DeviceEvents client connected")

	errCh := make(chan error, 1)
	s.deviceEventClients.Store(stream, errCh)
	defer s.deviceEventClients.Delete(stream)

	select {
	case <-s.stopChan:
		s.logger.Info("server stopping, closing DeviceEvents stream")
		return nil
	case <-stream.Context().Done():
		s.logger.Infow("stream closed", "error", stream.Context().Err())
		return stream.Context().Err()
	case e := <-errCh:
		s.logger.Infow("stopping DeviceEvents stream", "error", e)
		return e
	}
}

func (s *deviceInfoServer) broadcastEvents(ld plog.Logs) {
	events := translateEvents(ld, s.logger)
	if len(events) == 0 {
		return
	}
	s.deviceEventClients.Range(func(key, value any) bool {
		stream, ok := key.(pb.DeviceInfo_WatchDeviceEventsServer)
		if !ok {
			panic("invalid type assertion for device event client stream")
		}
		errCh, ok := value.(chan error)
		if !ok {
			panic("invalid type assertion for device event client stop channel")
		}
		for _, ev := range events {
			if err := stream.Send(ev); err != nil {
				s.logger.Errorw("failed to send event to client", "error", err)
				s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
				select {
				case errCh <- err:
				default:
				}
				return true
			}
			s.telemetry.ExporterRequestsSent.Add(context.Background(), 1)
		}
		return true
	})
}

func (s *deviceInfoServer) updateHealthData(md pmetric.Metrics) {
	s.healthDataLock.Lock()
	defer s.healthDataLock.Unlock()

	mt := newMetricsTranslator(s.logger, s.cfg)
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
