// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"encoding/xml"
	"fmt"
	"os"
	"strings"

	"cloudeng.io/text/linewrap"
)

// doxygen represents a Doxygen XML document.
type doxygen struct {
	Members []memberDef `xml:"compounddef>sectiondef>memberdef"`
	// names is a lookup-table from the C name to its corresponding member
	names map[string]memberBase
}

// memberBase is a base struct for common member and value fields
type memberBase struct {
	Name   string      `xml:"name"`
	Brief  description `xml:"briefdescription"`
	Detail description `xml:"detaileddescription"`
}

type memberDef struct {
	memberBase
	Kind  string       `xml:"kind,attr"`
	Enums []memberBase `xml:"enumvalue"`
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

func loadDoxygenXml(path string) (*doxygen, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	var doc doxygen
	if err := xml.Unmarshal(data, &doc); err != nil {
		return nil, err
	}

	doc.names, err = doc.membersMap()
	if err != nil {
		return nil, err
	}
	return &doc, nil
}

func (d *doxygen) getMemberByName(name string) *memberBase {
	m, ok := d.names[name]
	if !ok {
		return nil
	}
	return &m
}

// membersMap returns a name based lookup table (map) for the members.
// NOTE: We simplify and rely on the fact that there are no name clashes
// between members and enums.
func (d *doxygen) membersMap() (map[string]memberBase, error) {
	members := make(map[string]memberBase, len(d.Members))
	for _, member := range d.Members {
		if m, exists := members[member.Name]; exists {
			// We shouldn't have duplicate members with the same name.
			return nil, fmt.Errorf("clashing C name: %s (%s and %s)", member.Name, m.Name, member.Name)
		}
		members[member.Name] = member.memberBase

		// Add enum values if any
		for _, enum := range member.Enums {
			if _, exists := members[enum.Name]; exists {
				return nil, fmt.Errorf("clashing C name: %s (%s and %s)", enum.Name, enum.Name, member.Name)
			}
			members[enum.Name] = enum
		}
	}
	return members, nil
}

func (d *description) String() string {
	return d.Para.indentedString(0)
}

func (p *para) indentedString(indent int) string {
	var parts []string
	// Extract direct text content
	if t := strings.TrimSpace(p.Text); t != "" {
		// Wrap given indent+text around 80 cols (not strict limit)
		parts = append(parts, linewrap.Block(indent*2, 80, t))
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
