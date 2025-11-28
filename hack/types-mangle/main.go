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

type fieldMap map[string]string

type typeMap map[string]fieldMap

func main() {
	filePath := flag.String("file", "types.go", "Path to the Go source file to process")
	mappingsPath := flag.String("field-rewrites", "types-field-rewrites.yaml", "Path to the YAML file containing struct field type rewrite rules")
	flag.Parse()

	// Read mappings configuration
	mappings, err := loadMappings(*mappingsPath)
	if err != nil {
		log.Fatalf("Failed to load mappings: %v", err)
	}

	// Parse the Go source file
	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, *filePath, nil, parser.ParseComments)
	if err != nil {
		log.Fatal(err)
	}

	ast.Inspect(file, func(n ast.Node) bool {
		// Look for type declarations
		if typeSpec, ok := n.(*ast.TypeSpec); ok {
			structName := typeSpec.Name.Name
			if fieldMappings, ok := mappings[structName]; ok {
				// Check if it's a struct type
				if structType, ok := typeSpec.Type.(*ast.StructType); ok {
					// Iterate over the fields and update types as per the mapping
					for _, field := range structType.Fields.List {
						for _, fieldName := range field.Names {
							if newType, exists := fieldMappings[fieldName.Name]; exists {
								field.Type = ast.NewIdent(newType)
							}
						}
					}
				}
			}
		}
		return true
	})

	// Write back the modified file to stdout
	var buf bytes.Buffer
	if err := format.Node(&buf, fset, file); err != nil {
		log.Fatal(err)
	}

	_, _ = os.Stdout.Write(buf.Bytes())
}

func loadMappings(path string) (typeMap, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	mappings := typeMap{}
	if err := yaml.Unmarshal(data, &mappings); err != nil {
		return nil, err
	}
	return mappings, nil
}
