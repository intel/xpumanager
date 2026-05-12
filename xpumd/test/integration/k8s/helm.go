//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"context"
	"errors"
	"fmt"
	"path/filepath"
	"testing"
	"time"

	"helm.sh/helm/v4/pkg/action"
	"helm.sh/helm/v4/pkg/chart/common"
	"helm.sh/helm/v4/pkg/chart/loader"
	"helm.sh/helm/v4/pkg/kube"
	"helm.sh/helm/v4/pkg/storage/driver"
	"helm.sh/helm/v4/pkg/strvals"
	"k8s.io/cli-runtime/pkg/genericclioptions"
)

const helmInstallTimeout = 3 * time.Minute

type helmClient struct {
	namespace       string
	releaseName     string
	repoRoot        string
	valuesPath      string
	kubeconfigPath  string
	kubeContext     string
	imageRepository string
	imageTag        string
	imagePullPolicy string
}

// newHelmClient creates a helmClient capturing the test-specific fields and the
// current suite infrastructure settings (image, kubeconfig, etc.).
func newHelmClient(namespace, releaseName, valuesPath string) helmClient {
	return helmClient{
		namespace:       namespace,
		releaseName:     releaseName,
		valuesPath:      valuesPath,
		repoRoot:        suite.repoRoot,
		kubeconfigPath:  suite.kubeconfigPath,
		kubeContext:     suite.kubeContext,
		imageRepository: suite.imageRepository,
		imageTag:        suite.imageTag,
		imagePullPolicy: suite.imagePullPolicy,
	}
}

func (h helmClient) upgrade(t *testing.T) {
	t.Helper()

	cfg, err := h.actionCfg()
	if err != nil {
		t.Fatalf("failed to create helm action config: %v", err)
	}
	vals, err := h.mergeValues()
	if err != nil {
		t.Fatalf("failed to merge helm values: %v", err)
	}
	chrt, err := loader.Load(filepath.Join(h.repoRoot, "charts", "xpumd"))
	if err != nil {
		t.Fatalf("failed to load chart: %v", err)
	}

	install := action.NewInstall(cfg)
	install.ReleaseName = h.releaseName
	install.Namespace = h.namespace
	install.CreateNamespace = true
	install.WaitStrategy = kube.StatusWatcherStrategy
	install.Timeout = helmInstallTimeout
	install.TakeOwnership = true
	if _, err := install.RunWithContext(context.Background(), chrt, vals); err != nil {
		if !errors.Is(err, driver.ErrReleaseExists) {
			t.Fatalf("helm install failed: %v", err)
		}
		upgrade := action.NewUpgrade(cfg)
		upgrade.WaitStrategy = kube.StatusWatcherStrategy
		upgrade.Timeout = helmInstallTimeout
		if _, err := upgrade.Run(h.releaseName, chrt, vals); err != nil {
			t.Fatalf("helm upgrade failed: %v", err)
		}
	}
}

func (h helmClient) uninstall() error {
	cfg, err := h.actionCfg()
	if err != nil {
		return fmt.Errorf("failed to create helm action config: %w", err)
	}
	uninstall := action.NewUninstall(cfg)
	uninstall.WaitStrategy = kube.HookOnlyStrategy
	if _, err := uninstall.Run(h.releaseName); err != nil {
		return fmt.Errorf("helm uninstall failed: %w", err)
	}
	return nil
}

func (h helmClient) actionCfg() (*action.Configuration, error) {
	kubeconfig := h.kubeconfigPath
	kubecontext := h.kubeContext
	namespace := h.namespace
	f := genericclioptions.NewConfigFlags(true)
	f.KubeConfig = &kubeconfig
	f.Context = &kubecontext
	f.Namespace = &namespace

	cfg := new(action.Configuration)
	if err := cfg.Init(f, h.namespace, ""); err != nil {
		return nil, err
	}
	return cfg, nil
}

func (h helmClient) mergeValues() (map[string]interface{}, error) {
	vals, err := common.ReadValuesFile(h.valuesPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read values file: %w", err)
	}
	for _, overrides := range []string{
		fmt.Sprintf("image.repository=%s", h.imageRepository),
		fmt.Sprintf("image.tag=%s", h.imageTag),
		fmt.Sprintf("image.pullPolicy=%s", h.imagePullPolicy),
	} {
		// ParseStringInto preserves values as strings (e.g. not convert numeric image tags like "20260521" to numbers)
		if err := strvals.ParseIntoString(overrides, vals); err != nil {
			return nil, fmt.Errorf("failed to parse overrides: %w", err)
		}
	}
	return vals, nil
}
