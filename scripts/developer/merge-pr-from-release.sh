#!/bin/bash

if [ $# -ne 1 ]
then
  echo "[E] Usage: $0 [pull request number]"
  exit 1
fi

git fetch upstream

if [ "$(echo $(git status -s -b))" != "## master...upstream/master" ]
then
  echo "[W] Not being called from a local in-sync master branch"
  echo "      Press Ctrl+C to cancel operation or wait 5s to continue"
  sleep 5
fi

PR=$1
echo "[I] Locating PR merge commit in upstream/master"
PR_MERGE_COMMIT=$(git log --oneline --merges --first-parent --no-decorate upstream/release | awk -v PRMATCH="#${PR}" '$5==PRMATCH {print $1}')
if [ -z "${PR_MERGE_COMMIT}" ]
then
  echo "[E] Unable to find pull request #${PR}"
  exit 3
else
  echo "[I] Found ${PR_MERGE_COMMIT}"
fi

echo "[I] Locating PR base commit in upstream/master"
PR_BASE_COMMIT=$(git show -s --oneline ${PR_MERGE_COMMIT}^2 | awk '{print $1}')
echo "[I] Found ${PR_BASE_COMMIT}"

echo "[I] Merging base commit"
if ! git merge-base --is-ancestor ${PR_BASE_COMMIT} HEAD
then
  if ! git merge --no-ff --no-commit ${PR_BASE_COMMIT} 2>/dev/null
  then
    echo "[E] Merge failed"
    git reset --hard
    exit 4
  fi
  if ! git commit -C ${PR_MERGE_COMMIT}
  then
    echo "[E] Commit of previous merge failed"
    git reset --hard
    exit 5
  fi
else
  echo "[W] Skipping; branch already contains PR base commit ${PR_BASE_COMMIT}"
fi

echo "[I] Merging release branch"
if ! git merge --no-ff --no-commit ${PR_MERGE_COMMIT} 1>/dev/null 2>/dev/null
then
  echo "[E] Merge failed"
  git reset --hard
  exit 6
fi
if ! git commit -m "Merge release"
then
  echo "[E] Commit of previous merge failed"
  git reset --hard
  exit 7
fi
