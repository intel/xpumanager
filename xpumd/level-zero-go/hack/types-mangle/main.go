// Copyright (C) 2026 Intel Corporation
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
	"log/slog"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"text/template"

	"go.yaml.in/yaml/v4"
)

type config struct {
	Options options               `yaml:"options"`
	Structs []structRewriteConfig `yaml:"structs"`
	Types   []typeRewriteConfig   `yaml:"types"`
	Values  []valueRewriteConfig  `yaml:"values"`
}

type options struct {
	// Prefix to strip when converting C names to Go names.
	Prefix string `yaml:"prefix"`
	// DoxygenPath is the path to the Doxygen XML output file to read documentation from.
	DoxygenPath string `yaml:"doxygenPath"`
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
	commentRewriter `yaml:",inline"`
	Name            string `yaml:"name"`
	NewType         string `yaml:"newType"`
	TypeAssert      string `yaml:"typeAssert"`
}

type valueRewriteConfig struct {
	commentRewriter `yaml:",inline"`
	Name            string `yaml:"name"`
}

type commentRewriter struct {
	NewComment    string             `yaml:"newComment"`
	newCommentTpl *template.Template `yaml:"-"`
}

type typeRewriter struct {
	config *config
	dox    *doxygen
	cmap   ast.CommentMap
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

	var dox *doxygen
	if p := cfg.Options.DoxygenPath; p != "" {
		if !filepath.IsAbs(p) {
			p = filepath.Join(filepath.Dir(*mappingsPath), p)
		}
		slog.Info("Loading Doxygen XML", "path", p)
		dox, err = loadDoxygenXml(p, cfg.Options.Prefix)
		if err != nil {
			log.Fatalf("Failed to load Doxygen file %s: %v", p, err)
		}
	}

	for _, filePath := range args {
		if err := handleFile(filePath, *inPlace, cfg, dox); err != nil {
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

func handleFile(filePath string, inPlace bool, cfg *config, dox *doxygen) error {
	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, filePath, nil, parser.ParseComments)
	if err != nil {
		return fmt.Errorf("failed to parse file: %w", err)
	}

	cmap := ast.NewCommentMap(fset, file, file.Comments)
	trw := newTypeRewriter(cfg, dox, cmap)

	for _, decl := range file.Decls {
		genDecl, ok := decl.(*ast.GenDecl)
		if !ok {
			continue
		}
		if err := trw.handleDecl(genDecl); err != nil {
			return err
		}
	}

	// Update comments
	file.Comments = cmap.Comments()

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

func newTypeRewriter(cfg *config, dox *doxygen, cmap ast.CommentMap) *typeRewriter {
	return &typeRewriter{
		config: cfg,
		cmap:   cmap,
		dox:    dox,
	}
}

func (t *typeRewriter) handleDecl(genDecl *ast.GenDecl) error {
	for _, spec := range genDecl.Specs {
		switch v := spec.(type) {
		case *ast.TypeSpec:
			if err := t.handleType(genDecl, v); err != nil {
				return fmt.Errorf("failed to handle type %s: %w", v.Name.Name, err)
			}
		case *ast.ValueSpec:
			if err := t.handleValue(genDecl, v); err != nil {
				return fmt.Errorf("failed to handle value %v: %w", v.Names, err)
			}
		}
	}

	return nil
}

func (t *typeRewriter) handleType(genDecl *ast.GenDecl, typeSpec *ast.TypeSpec) error {
	typeName := typeSpec.Name.Name

	for _, typeRewrite := range t.config.Types {
		if matched, err := filepath.Match(typeRewrite.Name, typeName); err != nil {
			return fmt.Errorf("invalid type name pattern %q: %w", typeRewrite.Name, err)
		} else if matched {
			if err := t.rewriteType(&typeRewrite, genDecl, typeSpec); err != nil {
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

func (t *typeRewriter) handleValue(genDecl *ast.GenDecl, valueSpec *ast.ValueSpec) error {
	if len(valueSpec.Names) != 1 {
		slog.Warn("Skipping value rewrite for identifier list:", "names", valueSpec.Names)
		return nil
	}
	valueName := valueSpec.Names[0].Name

	for _, valueRewrite := range t.config.Values {
		if matched, err := filepath.Match(valueRewrite.Name, valueName); err != nil {
			return fmt.Errorf("invalid value name pattern %q: %w", valueRewrite.Name, err)
		} else if matched {
			if err := t.rewriteValue(&valueRewrite, genDecl, valueSpec); err != nil {
				return fmt.Errorf("failed to rewrite value: %w", err)
			}
		}
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

func (t *typeRewriter) rewriteType(tr *typeRewriteConfig, genDecl *ast.GenDecl, typeSpec *ast.TypeSpec) error {
	// Type assertion check
	if tr.TypeAssert != "" {
		typeIdent := types.ExprString(typeSpec.Type)
		if typeIdent != tr.TypeAssert {
			return fmt.Errorf("type assertion failed: expected %s, got %s", tr.TypeAssert, typeIdent)
		}
	}
	// Rewrite comment
	if tr.NewComment != "" {
		name := typeSpec.Name.Name
		vars := map[string]any{
			"Name":      name,
			"DocAnchor": typeNameToL0DocsAnchor(name),
		}
		if t.dox != nil {
			if m, found := t.dox.getMemberByGoName(name); found {
				vars["Doxygen"] = m
			}
		}
		if err := tr.rewriteComment(t.cmap, genDecl, typeSpec, vars); err != nil {
			return err
		}
	}
	// Rewrite type
	if tr.NewType != "" {
		typeSpec.Type = ast.NewIdent(tr.NewType)
	}
	return nil
}

func (t *typeRewriter) rewriteValue(tr *valueRewriteConfig, genDecl *ast.GenDecl, valueSpec *ast.ValueSpec) error {
	if tr.NewComment != "" {
		names := make([]string, len(valueSpec.Names))
		for i, n := range valueSpec.Names {
			names[i] = n.Name
		}
		vars := map[string]any{
			"Name0": names[0],
			"Names": names,
		}
		if t.dox != nil {
			if m, found := t.dox.getMemberByGoName(names[0]); found {
				vars["Doxygen"] = m
			}
		}
		return tr.rewriteComment(t.cmap, genDecl, valueSpec, vars)
	}

	return nil
}

func (cr *commentRewriter) rewriteComment(cmap ast.CommentMap, genDecl *ast.GenDecl, spec ast.Spec, vars map[string]any) error {
	if cr.newCommentTpl == nil {
		tpl, err := template.New("type-rewriter").Funcs(template.FuncMap{
			"doxygen": docFromDoxygen,
		}).Parse(cr.NewComment)
		if err != nil {
			return fmt.Errorf("failed to parse comment template: %w", err)
		}
		cr.newCommentTpl = tpl
	}
	var commentBuf strings.Builder
	if err := cr.newCommentTpl.Execute(&commentBuf, vars); err != nil {
		return fmt.Errorf("failed to execute comment template: %w", err)
	}
	// Add comment markers
	if text := strings.TrimSpace(commentBuf.String()); text != "" {
		lines := []string{}
		for l := range strings.SplitSeq(commentBuf.String(), "\n") {
			if l == "" {
				lines = append(lines, "//")
			} else {
				lines = append(lines, "// "+l)
			}
		}
		text = strings.Join(lines, "\n")

		if len(genDecl.Specs) > 1 {
			// For multi-spec declarations, attach comment to the spec
			pos := spec.Pos() - 1
			switch spec := spec.(type) {
			// Replace existing comment, if exists
			case *ast.TypeSpec:
				if spec.Doc != nil {
					pos = spec.Doc.Pos()
				}
			case *ast.ValueSpec:
				if spec.Doc != nil {
					pos = spec.Doc.Pos()
				}
			}
			newComment := newCommentGroup(text, pos)
			cmap[spec] = []*ast.CommentGroup{newComment}
			return nil
		} else {
			// For single-spec declarations, attach comment to the genDecl
			pos := genDecl.Pos() - 1
			// Replace existing comment, if exists
			if genDecl.Doc != nil {
				pos = genDecl.Doc.Pos()
			}
			newComment := newCommentGroup(text, pos)
			cmap[genDecl] = []*ast.CommentGroup{newComment}
		}
	}
	return nil
}

func newCommentGroup(text string, pos token.Pos) *ast.CommentGroup {
	return &ast.CommentGroup{
		List: []*ast.Comment{
			{
				Slash: pos,
				Text:  text,
			},
		},
	}
}

func typeNameToL0DocsAnchor(n string) string {
	if strings.HasSuffix(n, "Flag") {
		// The anchors of the singular flag types in the L0 docs are strange
		// unpredictable form like "#_CPPv426zes_device_property_flag_t". Thus,
		// use the anchor of the plural type instead.
		n = n + "s"
	}

	// Multiple capitals followed by lowercase
	re1 := regexp.MustCompile("([A-Z]+)([A-Z][a-z])")
	n = re1.ReplaceAllString(n, "${1}-${2}")

	// Lowercase or digit followed by capital
	re2 := regexp.MustCompile("([a-z0-9])([A-Z])")
	n = re2.ReplaceAllString(n, "${1}-${2}")

	return strings.ToLower(n) + "-t"
}

func docFromDoxygen(vars map[string]any) string {
	m, ok := vars["Doxygen"].(*memberBase)
	if !ok || m == nil {
		return ""
	}

	// Prefer detailed description
	doc := m.Detail.String()
	if doc == "" {
		doc = m.Brief.String()
	}
	return doc
}
