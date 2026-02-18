//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"gopkg.in/yaml.v3"

	pb "github.com/intel/xpumanager/exporter/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/exporter/internal/metadata"
)

var (
	socketDir  = flag.String("sock-dir", os.Getenv("XDG_RUNTIME_DIR"), "gRPC server socket directory")
	socketName = flag.String("sock-name", metadata.Type.String()+".sock", "gRPC server socket file name")
)

func main() {
	flag.Parse()

	os.Exit(run())
}

func run() (exitCode int) {
	conn, err := grpc.NewClient(
		"unix://"+filepath.Join(*socketDir, *socketName),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		slog.Error("failed to dial ", "error", err)
		return 1
	}
	defer conn.Close() // nolint:errcheck

	c := pb.NewDeviceInfoClient(conn)

	stream, err := c.WatchDeviceHealth(context.Background(), &pb.WatchDeviceHealthRequest{})
	if err != nil {
		slog.Error("error calling WatchDeviceHealth", "error", err)
		return 1
	}

	for {
		msg, err := stream.Recv()
		if err != nil {
			slog.Error("error receiving data", "error", err)
			return 1
		}
		data, err := yaml.Marshal(msg)
		if err != nil {
			slog.Error("failed to marshal device health data", "error", err)
			return 1
		}
		fmt.Println(string(data))
	}
}
