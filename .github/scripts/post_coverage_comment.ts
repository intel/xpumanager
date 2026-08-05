// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
//
// Posted as a PR comment by the coverage-measure workflow.
// Executed via actions/github-script (script-path), which injects:
//   github, context, core  — typed via @actions/github / @actions/core

import type { GitHub } from '@actions/github/lib/utils'
import type { Context } from '@actions/github/lib/context'
import type * as Core from '@actions/core'
import { spawnSync } from 'child_process'

declare const github: InstanceType<typeof GitHub>
declare const context: Context
declare const core: typeof Core

// Dynamic values passed through env: in the workflow step
const baseRef = process.env.BASE_REF
const prNumber = parseInt(process.env.PR_NUMBER ?? '', 10)

if (!baseRef) {
  core.setFailed('BASE_REF env var is not set.')
  return
}
if (isNaN(prNumber) || prNumber <= 0) {
  core.setFailed('No valid PR number available to post a comment on.')
  return
}

// Generate the markdown diff report, passing baseRef as an arg to avoid shell injection
const result = spawnSync(
  'python3',
  ['.github/scripts/compare_coverage.py', 'coverage_base.json', 'coverage_pr.json', '--base-ref', baseRef],
  { encoding: 'utf8', stdio: ['ignore', 'pipe', 'inherit'] },
)
if (result.status !== 0) {
  core.setFailed(`compare_coverage.py exited with status ${result.status}`)
  return
}
const reportBody = result.stdout

// Look up artifact IDs for direct HTML report download links
const { data: { artifacts } } = await github.rest.actions.listWorkflowRunArtifacts({
  owner: context.repo.owner,
  repo: context.repo.repo,
  run_id: context.runId,
})
const runUrl = `https://github.com/${context.repo.owner}/${context.repo.repo}/actions/runs/${context.runId}`
const links = (
  [
    ['coverage-pr',   'Head HTML report'],
    ['coverage-base', `${baseRef} baseline HTML report`],
  ] as const
)
  .map(([name, label]) => {
    const artifact = artifacts.find(a => a.name === name)
    return artifact ? `[${label}](${runUrl}/artifacts/${artifact.id})` : null
  })
  .filter((l): l is string => l !== null)
  .join(' | ')

const marker = '<!-- xpum-coverage-report -->'
const fullBody = `${marker}\n${reportBody}\n\n${links}`

const { data: comments } = await github.rest.issues.listComments({
  owner: context.repo.owner,
  repo: context.repo.repo,
  issue_number: prNumber,
})

const existing = comments.find(c => c.body?.includes(marker))
if (existing) {
  await github.rest.issues.updateComment({
    owner: context.repo.owner,
    repo: context.repo.repo,
    comment_id: existing.id,
    body: fullBody,
  })
  core.info(`Updated coverage comment (id ${existing.id}) on PR #${prNumber}`)
} else {
  await github.rest.issues.createComment({
    owner: context.repo.owner,
    repo: context.repo.repo,
    issue_number: prNumber,
    body: fullBody,
  })
  core.info(`Created coverage comment on PR #${prNumber}`)
}
