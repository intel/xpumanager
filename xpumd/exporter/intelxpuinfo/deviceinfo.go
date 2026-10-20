//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"sync"

	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha2"
	"github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/internal/metadata"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
)

// eventQueueSize is the maximum number of events queued for a single client.
const eventQueueSize = 1024

// healthClient represents one DeviceHealth subscriber.
type healthClient struct {
	// latest holds the latest health snapshot not yet sent to the client
	latest chan *pb.DeviceHealthResponse
}

// eventClient represents one DeviceEvents subscriber.
type eventClient struct {
	// queue holds the events not yet sent to the client
	queue chan *pb.DeviceEventResponse
}

type deviceInfoServer struct {
	pb.UnimplementedDeviceInfoServer
	logger    *zap.SugaredLogger
	telemetry *metadata.TelemetryBuilder
	cfg       *Config

	stopChan chan struct{}

	// healthLock protects the cached health data and the health clients
	healthLock sync.Mutex
	// Cached health data
	healthData *pb.DeviceHealthResponse
	// Active health clients
	healthClients map[*healthClient]struct{}

	// eventLock protects the event clients
	eventLock sync.Mutex
	// Active event clients
	eventClients map[*eventClient]struct{}
}

func newDeviceInfoServer(logger *zap.SugaredLogger, tb *metadata.TelemetryBuilder, cfg *Config) *deviceInfoServer {
	return &deviceInfoServer{
		logger:        logger,
		telemetry:     tb,
		cfg:           cfg,
		stopChan:      make(chan struct{}),
		healthClients: make(map[*healthClient]struct{}),
		eventClients:  make(map[*eventClient]struct{}),
	}
}

func (s *deviceInfoServer) WatchDeviceHealth(req *pb.WatchDeviceHealthRequest, stream pb.DeviceInfo_WatchDeviceHealthServer) error {
	s.logger.Info("new DeviceHealth client connected")

	c := &healthClient{latest: make(chan *pb.DeviceHealthResponse, 1)}

	// Register the client and queue the initial health data
	s.healthLock.Lock()
	s.healthClients[c] = struct{}{}
	if s.healthData != nil {
		c.updateData(s.healthData)
	}
	s.healthLock.Unlock()

	defer func() {
		s.healthLock.Lock()
		delete(s.healthClients, c)
		s.healthLock.Unlock()
	}()

	for {
		select {
		case <-s.stopChan:
			s.logger.Info("server stopping, closing DeviceHealth stream")
			return nil
		case <-stream.Context().Done():
			s.logger.Infow("DeviceHealth stream closed", "error", stream.Context().Err())
			return stream.Context().Err()
		case data := <-c.latest:
			s.logger.Debug("sending device health data")
			if err := stream.Send(data); err != nil {
				s.logger.Errorw("failed to send health data to client", "error", err)
				s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
				return err
			}
			s.telemetry.ExporterRequestsSent.Add(context.Background(), 1)
		}
	}
}

// updateData sends new health data (replacing pending data, if any).
func (c *healthClient) updateData(data *pb.DeviceHealthResponse) {
	select {
	case <-c.latest:
	default:
	}
	// NOTE: Each update is a full snapshot of all devices so simple replacement is safe
	c.latest <- data
}

func (s *deviceInfoServer) stop() {
	close(s.stopChan)
}

func (s *deviceInfoServer) WatchDeviceEvents(req *pb.WatchDeviceEventsRequest, stream pb.DeviceInfo_WatchDeviceEventsServer) error {
	s.logger.Info("new DeviceEvents client connected")

	c := &eventClient{queue: make(chan *pb.DeviceEventResponse, eventQueueSize)}

	s.eventLock.Lock()
	s.eventClients[c] = struct{}{}
	s.eventLock.Unlock()

	defer func() {
		s.eventLock.Lock()
		delete(s.eventClients, c)
		s.eventLock.Unlock()
	}()

	for {
		select {
		case <-s.stopChan:
			s.logger.Info("server stopping, closing DeviceEvents stream")
			return nil
		case <-stream.Context().Done():
			s.logger.Infow("DeviceEvents stream closed", "error", stream.Context().Err())
			return stream.Context().Err()
		case ev := <-c.queue:
			if err := stream.Send(ev); err != nil {
				s.logger.Errorw("failed to send event to client", "error", err)
				s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
				return err
			}
			s.telemetry.ExporterRequestsSent.Add(context.Background(), 1)
		}
	}
}

func (s *deviceInfoServer) broadcastEvents(ld plog.Logs) {
	events := translateEvents(ld, s.logger)
	if len(events) == 0 {
		return
	}

	s.eventLock.Lock()
	defer s.eventLock.Unlock()
	for c := range s.eventClients {
		if dropped := c.enqueue(events); dropped > 0 {
			s.logger.Warnw("DeviceEvents client too slow, event queue full, dropping events", "dropped", dropped)
			s.telemetry.ExporterRequestErrors.Add(context.Background(), int64(dropped))
		}
	}
}

// enqueue queues the events for the client. Returns the number of events
// dropped because the queue is full.
func (c *eventClient) enqueue(events []*pb.DeviceEventResponse) int {
	for i, ev := range events {
		select {
		case c.queue <- ev:
		default:
			return len(events) - i
		}
	}
	return 0
}

func (s *deviceInfoServer) updateHealthData(md pmetric.Metrics) {
	mt := newMetricsTranslator(s.logger, s.cfg)
	s.publishHealthData(mt.translate(md))
}

func (s *deviceInfoServer) publishHealthData(data *pb.DeviceHealthResponse) {
	s.healthLock.Lock()
	defer s.healthLock.Unlock()
	s.healthData = data
	for c := range s.healthClients {
		c.updateData(data)
	}
}
