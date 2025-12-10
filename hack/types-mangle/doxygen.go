// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"encoding/xml"
	"fmt"
	"os"
	"strings"
)

// doxygen represents a Doxygen XML document.
type doxygen struct {
	Members      []memberDef `xml:"compounddef>sectiondef>memberdef"`
	golanMembers map[string]memberDef
}

type memberDef struct {
	Kind   string      `xml:"kind,attr"`
	Name   string      `xml:"name"`
	Brief  description `xml:"briefdescription"`
	Detail description `xml:"detaileddescription"`
}

type description struct {
	Para paraList `xml:"para"`
}

type paraList []para

type para struct {
	Text         string        `xml:",chardata"`
	ItemizedList *itemizedList `xml:"itemizedlist"`
}

type itemizedList struct {
	ListItems []listItem `xml:"listitem"`
}

type listItem struct {
	Para paraList `xml:"para"`
}

func loadDoxygenXml(path, prefix string) (*doxygen, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	var doc doxygen
	if err := xml.Unmarshal(data, &doc); err != nil {
		return nil, err
	}

	doc.golanMembers, err = doc.membersMap(prefix)
	if err != nil {
		return nil, err
	}
	return &doc, nil
}

func (d *doxygen) getMemberByGoName(goName string) (*memberDef, bool) {
	m, ok := d.golanMembers[goName]
	if !ok {
		return nil, false
	}
	return &m, true
}

// membersMap returns a map of golang-name to memberDef.
func (d *doxygen) membersMap(prefix string) (map[string]memberDef, error) {
	members := make(map[string]memberDef, len(d.Members))
	for _, member := range d.Members {
		gn := member.goName(prefix)
		if m, exists := members[gn]; exists {
			// We shouldn't have duplicate members with the same name.
			return nil, fmt.Errorf("clashing go name: %s (%s and %s)", gn, m.Name, member.Name)
		}
		members[gn] = member
	}
	return members, nil
}

func (m *memberDef) goName(prefix string) string {
	switch m.Kind {
	case "function":
		// Expect the function names not to be mangled
		return m.Name
	case "define": // like ZES_ENGINE_TYPE_FLAG_RENDER
		return strings.TrimPrefix(m.Name, strings.ToUpper(prefix)+"_")
	case "enum": // _zes_engine_type_flag_t
		trimmed := strings.TrimSuffix(strings.TrimPrefix(m.Name, "_"+prefix+"_"), "_t")
		return "_" + snakeToCamel(trimmed)
	case "typedef": // like zes_engine_type_flags_t
		trimmed := strings.TrimSuffix(strings.TrimPrefix(m.Name, prefix+"_"), "_t")
		return snakeToCamel(trimmed)
	default:
		return ""
	}
}

func (d *description) String() string {
	return d.Para.indentedString(0)
}

func (p *para) indentedString(indent int) string {
	var parts []string
	indentStr := strings.Repeat("  ", indent)

	// Extract direct text content
	if t := strings.TrimSpace(p.Text); t != "" {
		parts = append(parts, indentStr+t)
	}

	// Handle itemized lists
	if p.ItemizedList != nil {
		parts = append(parts, p.ItemizedList.indentedString(indent))
	}
	return strings.Join(parts, "\n")
}

func (l *itemizedList) indentedString(indent int) string {
	var items []string
	for _, item := range l.ListItems {
		for i, p := range item.Para {
			itemText := p.indentedString(indent + 1)
			if itemText != "" {
				if i == 0 {
					// Add bullet for the first paragraph
					itemText = itemText[:indent*2] + "-" + itemText[indent*2+1:]
				}
				items = append(items, itemText)
			}
		}
	}
	return strings.Join(items, "\n")
}

func (l *paraList) indentedString(indent int) string {
	var parts []string
	for _, p := range *l {
		pStr := p.indentedString(indent)
		if pStr != "" {
			parts = append(parts, pStr)
		}
	}
	return strings.Join(parts, "\n\n")
}

func snakeToCamel(s string) string {
	parts := strings.Split(s, "_")
	for i, part := range parts {
		parts[i] = strings.ToUpper(part[:1]) + part[1:]
	}
	return strings.Join(parts, "")
}
