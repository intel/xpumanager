// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"bytes"
	"flag"
	"go/ast"
	"go/format"
	"go/parser"
	"go/token"
	"log"
	"os"

	"go.yaml.in/yaml/v4"
)

type config struct {
	Structs []structRewriteConfig `yaml:"structs"`
}

// structRewriteConfig holds the configuration mangling struct types
type structRewriteConfig struct {
	Name   string               `yaml:"name"`
	Fields []fieldRewriteConfig `yaml:"fields"`
}

// fieldRewriteConfig holds the configuration for mangling a specific field of
// a struct
type fieldRewriteConfig struct {
	Name    string `yaml:"name"`
	NewName string `yaml:"newName"`
	NewType string `yaml:"newType"`
}

type typeRewriter struct {
	config *config
}

func main() {
	filePath := flag.String("file", "types.go", "Path to the Go source file to process")
	mappingsPath := flag.String("config", "types-mangle.yaml", "Path to the type rewrite rules config file")
	flag.Parse()

	// Read configuration
	cfg, err := loadConfig(*mappingsPath)
	if err != nil {
		log.Fatalf("Failed to load config: %v", err)
	}
	trw := newTypeRewriter(cfg)

	// Parse the Go source file
	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, *filePath, nil, parser.ParseComments)
	if err != nil {
		log.Fatal(err)
	}

	for _, decl := range file.Decls {
		genDecl, ok := decl.(*ast.GenDecl)
		if !ok {
			continue
		}
		for _, spec := range genDecl.Specs {
			// Look for type declarations
			if typeSpec, ok := spec.(*ast.TypeSpec); ok {
				trw.handleType(typeSpec)
			}
		}
	}

	// Write back the modified file to stdout
	var buf bytes.Buffer
	if err := format.Node(&buf, fset, file); err != nil {
		log.Fatal(err)
	}

	_, _ = os.Stdout.Write(buf.Bytes())
}

func loadConfig(path string) (*config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	cfg := &config{}
	if err := yaml.Unmarshal(data, cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}

func newTypeRewriter(cfg *config) *typeRewriter {
	return &typeRewriter{config: cfg}
}

func (t *typeRewriter) handleType(typeSpec *ast.TypeSpec) {
	typeName := typeSpec.Name.Name

	switch v := typeSpec.Type.(type) {
	case *ast.StructType:
		for _, structRewrite := range t.config.Structs {
			if structRewrite.Name == typeName {
				structRewrite.apply(v)
				return
			}
		}
	default:
	}

}

func (s *structRewriteConfig) apply(structType *ast.StructType) {
	fieldRewrites := make(map[string]fieldRewriteConfig)
	for _, field := range s.Fields {
		fieldRewrites[field.Name] = field
	}

	for _, field := range structType.Fields.List {
		for _, fieldName := range field.Names {
			if fieldRewrite, ok := fieldRewrites[fieldName.Name]; ok {
				fieldRewrite.apply(field)
			}
		}
	}
}

func (f *fieldRewriteConfig) apply(field *ast.Field) {
	if f.NewName != "" {
		for _, name := range field.Names {
			name.Name = f.NewName
		}
	}
	if f.NewType != "" {
		field.Type = ast.NewIdent(f.NewType)
	}
}
