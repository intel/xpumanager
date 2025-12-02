//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"gopkg.in/yaml.v3"

	pb "github.com/intel/xpumanager/exporter/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/exporter/internal/metadata"
)

var (
	defaultSocketFilename = metadata.Type.String() + ".sock"
	socketPath            = flag.String("socket", "/run/xpumd/"+defaultSocketFilename, "Path to the socket for the gRPC server")
)

func main() {
	flag.Parse()

	os.Exit(run())
}

func run() (exitCode int) {
	conn, err := grpc.NewClient(
		"unix://"+*socketPath,
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
