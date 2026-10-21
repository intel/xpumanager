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
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// eventQueueSize is the maximum number of events queued for a single client.
const eventQueueSize = 1024

// healthMaxSkipped is the number of consecutive health updates a client may
// miss before it is considered stalled.
const healthMaxSkipped = 10

// healthClient represents one DeviceHealth subscriber.
type healthClient struct {
	// latest holds the latest health snapshot not yet sent to the client
	latest chan *pb.DeviceHealthResponse
	// skipped counts consecutive updates skipped because the client didn't read them fast enough
	skipped int
	// stalled signals that the client stopped receiving updates
	stalled chan struct{}
}

// eventClient represents one DeviceEvents subscriber.
type eventClient struct {
	// queue holds the events not yet sent to the client
	queue chan *pb.DeviceEventResponse
	// overflow signals a queue overflow
	overflow chan struct{}
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

	c := &healthClient{
		latest:  make(chan *pb.DeviceHealthResponse, 1),
		stalled: make(chan struct{}),
	}

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

	sendErr := make(chan error, 1)
	go func() { sendErr <- s.sendHealthData(stream, c) }()

	select {
	case <-s.stopChan:
		s.logger.Info("server stopping, closing DeviceHealth stream")
		return nil
	case <-stream.Context().Done():
		s.logger.Infow("DeviceHealth stream closed", "error", stream.Context().Err())
		return stream.Context().Err()
	case <-c.stalled:
		s.logger.Warn("DeviceHealth client stalled, closing stream")
		return status.Error(codes.ResourceExhausted, "client stalled, health updates not received")
	case err := <-sendErr:
		return err
	}
}

func (s *deviceInfoServer) sendHealthData(stream pb.DeviceInfo_WatchDeviceHealthServer, c *healthClient) error {
	ctx := stream.Context()
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case data := <-c.latest:
			s.logger.Debug("sending device health data")
			if err := stream.Send(data); err != nil {
				if ctx.Err() != nil {
					// The stream ended while sending
					return ctx.Err()
				}
				s.logger.Errorw("failed to send health data to client", "error", err)
				s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
				return err
			}
			s.telemetry.ExporterRequestsSent.Add(context.Background(), 1)
		}
	}
}

// updateData sends new health data (replacing pending data, if any).
// Returns false if the client is stalled.
func (c *healthClient) updateData(data *pb.DeviceHealthResponse) bool {
	select {
	case <-c.latest:
		c.skipped++
	default:
		c.skipped = 0
	}
	// NOTE: Each update is a full snapshot of all devices so simple replacement is safe
	c.latest <- data
	return c.skipped < healthMaxSkipped
}

func (s *deviceInfoServer) stop() {
	close(s.stopChan)
}

func (s *deviceInfoServer) WatchDeviceEvents(req *pb.WatchDeviceEventsRequest, stream pb.DeviceInfo_WatchDeviceEventsServer) error {
	s.logger.Info("new DeviceEvents client connected")

	c := &eventClient{
		queue:    make(chan *pb.DeviceEventResponse, eventQueueSize),
		overflow: make(chan struct{}),
	}

	s.eventLock.Lock()
	s.eventClients[c] = struct{}{}
	s.eventLock.Unlock()

	defer func() {
		s.eventLock.Lock()
		delete(s.eventClients, c)
		s.eventLock.Unlock()
	}()

	sendErr := make(chan error, 1)
	go func() { sendErr <- s.sendEvents(stream, c) }()

	select {
	case <-s.stopChan:
		s.logger.Info("server stopping, closing DeviceEvents stream")
		return nil
	case <-stream.Context().Done():
		s.logger.Infow("DeviceEvents stream closed", "error", stream.Context().Err())
		return stream.Context().Err()
	case <-c.overflow:
		s.logger.Warn("DeviceEvents client too slow, event queue overflow, closing stream")
		return status.Error(codes.ResourceExhausted, "client too slow, event queue full")
	case err := <-sendErr:
		return err
	}
}

func (s *deviceInfoServer) sendEvents(stream pb.DeviceInfo_WatchDeviceEventsServer, c *eventClient) error {
	ctx := stream.Context()
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case ev := <-c.queue:
			if err := stream.Send(ev); err != nil {
				if ctx.Err() != nil {
					// The stream ended while sending
					return ctx.Err()
				}
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
		if !c.enqueue(events) {
			// Removed from the registry, so overflow is closed only once
			delete(s.eventClients, c)
			close(c.overflow)
			s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
		}
	}
}

// enqueue queues the events for the client. Returns false if the queue overflows.
func (c *eventClient) enqueue(events []*pb.DeviceEventResponse) bool {
	for _, ev := range events {
		select {
		case c.queue <- ev:
		default:
			return false
		}
	}
	return true
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
		if !c.updateData(data) {
			// Removed from the registry, so stalled is closed only once
			delete(s.healthClients, c)
			close(c.stalled)
			s.telemetry.ExporterRequestErrors.Add(context.Background(), 1)
		}
	}
}
