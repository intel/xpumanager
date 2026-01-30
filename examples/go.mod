module github.com/intel/level-zero-go/examples

go 1.25

replace github.com/intel/level-zero-go => ../

require (
	github.com/intel/level-zero-go v0.0.0-00010101000000-000000000000
	gopkg.in/yaml.v3 v3.0.1
)

require github.com/google/uuid v1.6.0 // indirect
