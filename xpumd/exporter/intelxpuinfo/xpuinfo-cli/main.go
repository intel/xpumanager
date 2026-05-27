//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package main

import (
	"context"
	"flag"
	"io"
	"log"
	"log/slog"
	"os"
	"path/filepath"
	"sync"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"gopkg.in/yaml.v3"

	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/internal/metadata"
)

var (
	// Use logger for thread-safe output
	stdout = log.New(os.Stdout, "", 0)

	socketDir  = flag.String("sock-dir", os.Getenv("XDG_RUNTIME_DIR"), "gRPC server socket directory")
	socketName = flag.String("sock-name", metadata.Type.String()+".sock", "gRPC server socket file name")
	noHealth   = flag.Bool("no-health", false, "disable device health stream")
	noEvents   = flag.Bool("no-events", false, "disable device events stream")
)

func main() {
	flag.Parse()

	os.Exit(run())
}

func run() int {
	conn, err := grpc.NewClient(
		"unix://"+filepath.Join(*socketDir, *socketName),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		slog.Error("failed to dial", "error", err)
		return 1
	}
	defer conn.Close() // nolint:errcheck

	if *noHealth && *noEvents {
		slog.Error("both streams disabled, nothing to do")
		return 1
	}

	c := pb.NewDeviceInfoClient(conn)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	errc := make(chan int, 2)
	var wg sync.WaitGroup

	if !*noHealth {
		wg.Go(func() {
			errc <- runStream(ctx, "health", func(ctx context.Context) (grpcStream[*pb.DeviceHealthResponse], error) {
				return c.WatchDeviceHealth(ctx, &pb.WatchDeviceHealthRequest{})
			})
			cancel()
		})
	}
	if !*noEvents {
		wg.Go(func() {
			errc <- runStream(ctx, "events", func(ctx context.Context) (grpcStream[*pb.DeviceEventResponse], error) {
				return c.WatchDeviceEvents(ctx, &pb.WatchDeviceEventsRequest{})
			})
			cancel()
		})
	}

	wg.Wait()
	close(errc)

	for code := range errc {
		if code != 0 {
			return code
		}
	}
	return 0
}

type grpcStream[T any] interface {
	Recv() (T, error)
}

func runStream[T any](ctx context.Context, name string, open func(context.Context) (grpcStream[T], error)) int {
	stream, err := open(ctx)
	if err != nil {
		if ctx.Err() != nil {
			return 0
		}
		slog.Error("error opening stream", "stream", name, "error", err)
		return 1
	}

	for {
		msg, err := stream.Recv()
		if err != nil {
			code := status.Code(err)
			switch {
			case err == io.EOF || code == codes.OK:
				slog.Info("stream closed by server", "stream", name)
				return 0
			case code == codes.Canceled:
				slog.Info("stream canceled", "stream", name)
				return 0
			default:
				slog.Error("error receiving from stream", "stream", name, "error", err, "grpcCode", code)
				return 1
			}
		}
		data, err := yaml.Marshal(msg)
		if err != nil {
			slog.Error("failed to marshal message", "stream", name, "error", err)
			return 1
		}
		stdout.Print(string(data))
	}
}
