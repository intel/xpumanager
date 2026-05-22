module github.com/intel/level-zero-go/examples

go 1.26

replace github.com/intel/level-zero-go => ../

require (
	github.com/intel/level-zero-go v0.0.0-00010101000000-000000000000
	go.yaml.in/yaml/v4 v4.0.0-rc.4
)

require github.com/google/uuid v1.6.0 // indirect
