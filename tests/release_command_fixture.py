#!/usr/bin/env python3
"""Offline git/gh/git-cliff stand-ins for release automation tests only."""
import json
import os
from pathlib import Path
import shutil
import sys

root = Path(os.environ["RELEASE_TEST_ROOT"])
state_file = root / "state.json"
state = json.loads(state_file.read_text())
command = Path(sys.argv[0]).name
args = sys.argv[1:]
state.setdefault("events", []).append([command, *args])


def fail(message):
    print(message, file=sys.stderr)
    return 1


def main():
    if command == "git":
        if args[0] == "-C" and args[2:] == ["rev-parse", "--show-toplevel"]:
            print(root)
        elif args == ["rev-parse", "HEAD"]:
            print("a" * 40)
        elif args[:3] == ["show-ref", "--verify", "--quiet"]:
            return 0 if state.get("tag") else 1
        elif args[0] == "rev-parse":
            print(state["tag"])
        elif args[:2] == ["cat-file", "-t"]:
            print(state.get("tag_type", "tag"))
        elif args[0] == "config":
            pass
        elif args[:2] == ["tag", "--annotate"]:
            state["tag"] = "a" * 40
        elif args[0] == "push":
            if state.get("fail_push"):
                return fail("fixture push failure")
            state["remote_tag"] = state["tag"]
        else:
            return fail(f"unexpected git arguments: {args}")
        return 0
    if command == "git-cliff":
        if state.get("fail_notes"):
            return fail("fixture changelog failure")
        print("# Fixture changelog")
        return 0
    if command != "gh":
        return fail(f"unexpected fixture command: {command}")
    if args[0] == "api":
        if state.get("fail_api"):
            return fail("fixture API failure")
        print(state.get("release", ""))
    elif args[:2] == ["pr", "list"]:
        if os.environ.get("GH_TOKEN") != "fixture-bot-token" or state.get("fail_bot"):
            return fail("fixture bot authentication failure")
        print(state.get("pr", ""))
    elif args[:2] == ["pr", "create"]:
        assert os.environ["GH_TOKEN"] == "fixture-bot-token"
        assert state["release"] == "published"
        assert "--base" in args and args[args.index("--base") + 1] == "develop"
        assert args[args.index("--head") + 1] == "main"
        if state.get("fail_backmerge"):
            return fail("fixture back-merge failure")
        state["pr"] = "42"
    elif args[:2] == ["release", "create"]:
        assert "--draft" in args and "--verify-tag" in args
        assert state.get("remote_tag") == "a" * 40
        assert not state.get("release")
        if state.get("fail_create"):
            return fail("fixture draft creation failure")
        notes = Path(args[args.index("--notes-file") + 1]).read_text()
        assert "Release overview" in notes and "Fixture changelog" in notes
        state["release"] = "draft"
    elif args[:2] == ["release", "view"]:
        if state.get("publish_race"):
            state["release"] = "published"
        print("true" if state.get("release") == "draft" else "false")
    elif args[:2] == ["release", "upload"]:
        assert state["release"] == "draft"
        assert "--clobber" in args
        sources = [Path(arg) for arg in args[3:] if Path(arg).is_file()]
        assert len(sources) == 2
        remote = root / "remote"
        remote.mkdir(exist_ok=True)
        for source in sources:
            shutil.copyfile(source, remote / source.name)
            if state.pop("fail_upload_once", False):
                return fail("fixture interrupted upload")
    elif args[:2] == ["release", "download"]:
        if state.get("fail_download"):
            return fail("fixture download failure")
        destination = Path(args[args.index("--dir") + 1])
        for index, arg in enumerate(args):
            if arg != "--pattern":
                continue
            filename = args[index + 1]
            source = root / "remote" / filename
            if not source.is_file():
                return fail("fixture missing asset")
            shutil.copyfile(source, destination / filename)
            if state.get("corrupt_download") and filename.endswith(".tar.gz"):
                with (destination / filename).open("ab") as stream:
                    stream.write(b"corruption")
    elif args[:2] == ["release", "edit"]:
        assert state["release"] == "draft" and "--draft=false" in args
        if state.get("fail_publish"):
            return fail("fixture publication failure")
        state["release"] = "published"
    else:
        return fail(f"unexpected gh arguments: {args}")
    return 0


try:
    result = main()
finally:
    state_file.write_text(json.dumps(state))
sys.exit(result)
