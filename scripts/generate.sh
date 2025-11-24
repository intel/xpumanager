#!/bin/bash -ex
cd charts/xpu-exporter
go tool helm-docs
go tool helm-values-schema-json
