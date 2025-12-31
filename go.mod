module github.com/intel/xpumanager

go 1.25.0

require (
	github.com/intel/level-zero-go v0.0.0
	github.com/stretchr/testify v1.11.1
	go.opentelemetry.io/collector/consumer v1.48.0
	go.opentelemetry.io/collector/pdata v1.48.0
	go.uber.org/zap v1.27.1
)

require (
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/hashicorp/go-version v1.8.0 // indirect
	github.com/json-iterator/go v1.1.12 // indirect
	github.com/modern-go/concurrent v0.0.0-20180306012644-bacd9c7ef1dd // indirect
	github.com/modern-go/reflect2 v1.0.3-0.20250322232337-35a7c28c31ee // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	go.opentelemetry.io/collector/featuregate v1.48.0 // indirect
	go.uber.org/multierr v1.11.0 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

// HACK: use local level-zero-go module until published
replace github.com/intel/level-zero-go => ./level-zero-go
