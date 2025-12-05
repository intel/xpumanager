// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"bytes"
	"flag"
	"fmt"
	"go/ast"
	"go/format"
	"go/parser"
	"go/token"
	"go/types"
	"log"
	"os"
	"path/filepath"

	"go.yaml.in/yaml/v4"
)

type config struct {
	Structs []structRewriteConfig `yaml:"structs"`
	Types   []typeRewriteConfig   `yaml:"types"`
}

// structRewriteConfig holds the configuration mangling struct types
type structRewriteConfig struct {
	Name   string               `yaml:"name"`
	Fields []fieldRewriteConfig `yaml:"fields"`
}

// fieldRewriteConfig holds the configuration for mangling a specific field of
// a struct
type fieldRewriteConfig struct {
	Name       string `yaml:"name"`
	NewName    string `yaml:"newName"`
	NewType    string `yaml:"newType"`
	TypeAssert string `yaml:"typeAssert"`
}

type typeRewriteConfig struct {
	Name    string `yaml:"name"`
	NewType string `yaml:"newType"`
}

type typeRewriter struct {
	config *config
}

func main() {
	mappingsPath := flag.String("config", "types-mangle.yaml", "Path to the type rewrite rules config file")
	inPlace := flag.Bool("in-place", false, "Modify the file in place")
	flag.Parse()

	args := flag.Args()
	if len(args) == 0 {
		log.Fatal("No source files specified")
	}

	// Read configuration
	cfg, err := loadConfig(*mappingsPath)
	if err != nil {
		log.Fatalf("Failed to load config: %v", err)
	}

	for _, filePath := range args {
		if err := handleFile(filePath, *inPlace, cfg); err != nil {
			log.Fatalf("Failed to process file %s: %v", filePath, err)
		}
	}
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

func handleFile(filePath string, inPlace bool, cfg *config) error {
	trw := newTypeRewriter(cfg)

	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, filePath, nil, parser.ParseComments)
	if err != nil {
		return fmt.Errorf("failed to parse file: %w", err)
	}

	for _, decl := range file.Decls {
		genDecl, ok := decl.(*ast.GenDecl)
		if !ok {
			continue
		}
		for _, spec := range genDecl.Specs {
			// Look for type declarations
			if typeSpec, ok := spec.(*ast.TypeSpec); ok {
				if err := trw.handleType(typeSpec); err != nil {
					return fmt.Errorf("failed to handle type %s: %w", typeSpec.Name.Name, err)
				}
			}
		}
	}

	var buf bytes.Buffer
	if err := format.Node(&buf, fset, file); err != nil {
		log.Fatal(err)
	}

	if inPlace {
		if err := os.WriteFile(filePath, buf.Bytes(), 0644); err != nil {
			return fmt.Errorf("failed to write modified file: %w", err)
		}
	} else {
		_, _ = os.Stdout.Write(buf.Bytes())
	}
	return nil
}

func newTypeRewriter(cfg *config) *typeRewriter {
	return &typeRewriter{config: cfg}
}

func (t *typeRewriter) handleType(typeSpec *ast.TypeSpec) error {
	typeName := typeSpec.Name.Name

	for _, typeRewrite := range t.config.Types {
		if matched, err := filepath.Match(typeRewrite.Name, typeName); err != nil {
			return fmt.Errorf("invalid type name pattern %q: %w", typeRewrite.Name, err)
		} else if matched {
			if err := typeRewrite.apply(typeSpec); err != nil {
				return fmt.Errorf("failed to rewrite type: %w", err)
			}
		}
	}

	switch v := typeSpec.Type.(type) {
	case *ast.StructType:
		for _, structRewrite := range t.config.Structs {
			if matched, err := filepath.Match(structRewrite.Name, typeName); err != nil {
				return fmt.Errorf("invalid struct name pattern %q: %w", structRewrite.Name, err)
			} else if matched {
				if err := structRewrite.apply(v); err != nil {
					return fmt.Errorf("failed to rewrite struct: %w", err)
				}
			}
		}
	default:
	}
	return nil
}

func (s *structRewriteConfig) apply(structType *ast.StructType) error {
	fieldRewrites := make(map[string]fieldRewriteConfig)
	for _, field := range s.Fields {
		fieldRewrites[field.Name] = field
	}

	for _, field := range structType.Fields.List {
		for _, fieldName := range field.Names {
			if fieldRewrite, ok := fieldRewrites[fieldName.Name]; ok {
				if err := fieldRewrite.apply(field); err != nil {
					return fmt.Errorf("failed to rewrite field %s: %w", fieldName.Name, err)
				}
			}
		}
	}
	return nil
}

func (f *fieldRewriteConfig) apply(field *ast.Field) error {
	if len(field.Names) > 1 {
		return fmt.Errorf("field rewrite cannot be applied to identifier lists")
	}
	if f.TypeAssert != "" {
		typeIdent := types.ExprString(field.Type)
		if typeIdent != f.TypeAssert {
			return fmt.Errorf("field type assertion failed: expected %s, got %s", f.TypeAssert, typeIdent)
		}
	}
	if f.NewName != "" {
		field.Names[0].Name = f.NewName
	}
	if f.NewType != "" {
		field.Type = ast.NewIdent(f.NewType)
	}

	return nil
}

func (t *typeRewriteConfig) apply(typeSpec *ast.TypeSpec) error {
	if t.NewType != "" {
		typeSpec.Type = ast.NewIdent(t.NewType)
	}
	return nil
}
