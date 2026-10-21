#!/usr/bin/env python3
"""Exercise the public scripts without installing packages or touching the host.

Package-manager/compiler fixtures test the platform branches. Git update tests
use real local repositories, including failed fetches and dirty/diverged trees.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
SH = shutil.which("sh")
GIT = shutil.which("git")
MANAGERS = ("apt-get", "dnf", "yum", "pacman", "zypper", "apk",
            "xbps-install", "eopkg", "emerge", "pkg", "pkg_add", "pkgin",
            "brew", "port")

STUB = r'''
import json, os, pathlib, shutil, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
env = os.environ
state = pathlib.Path(env["TEST_STATE"])
with open(env["TEST_CALLS"], "a") as log:
    log.write(json.dumps([name, args, env.get("TEST_ELEVATED", "0")]) + "\n")
if name == "id":
    print(env.get("TEST_UID", "0"))
elif name == "uname":
    print(env.get("TEST_OS", "Linux"))
elif name in ("sudo", "doas"):
    if env.get("TEST_AUTH_FAIL"):
        sys.exit("fixture authentication failed")
    if args == ["-v"]:
        sys.exit(0)
    if args[0] == "-n":
        args = args[1:]
    os.environ["TEST_ELEVATED"] = "1"
    os.execvpe(args[0], args, os.environ)
elif name in ("c++", "g++", "clang++"):
    if env.get("TEST_MISSING_BUILD") and (not state.exists() or env.get("TEST_STILL_BROKEN")):
        sys.exit("fixture compiler is not usable")
    if any(arg.endswith("/cxx.cpp") for arg in args):
        if env.get("TEST_FS_LIBRARY") and "-lstdc++fs" not in args:
            sys.exit("fixture needs the filesystem library")
    else:
        library = env.get("TEST_LUA_LIBRARY", "-llua_fixture")
        if library not in args:
            sys.exit("fixture Lua link failed")
elif name in ("pkg-config", "pkgconf"):
    module = env.get("TEST_LUA_MODULE", "lua5.4")
    if env.get("TEST_PC_EMPTY"):
        sys.exit(1)
    bad = args[-1] == "lua5.5" and env.get("TEST_BAD_LUA_FIRST")
    if args[0] == "--exists":
        sys.exit(0 if args[-1] == module or bad else 1)
    if args[0] == "--cflags":
        print("-I/fixture/include")
    elif args[0] == "--libs":
        print("-llua_bad" if bad else "-llua_fixture")
elif name in ("make", "gmake"):
    if args == ["--version"]:
        print("GNU Make 4.4")
    elif env.get("TEST_BUILD_FAIL"):
        sys.exit("fixture build failed")
    else:
        assert "all" in args and "-B" in args, args
        assert sys.stdin.read() == "", "make consumed the installer"
        binary = pathlib.Path("tedit")
        binary.write_text("#!/bin/sh\nprintf 'tedit fixture\\n'\n")
        binary.chmod(0o755)
elif name == "git":
    assert args[0] == "clone", args
    if env.get("TEST_CLONE_FAIL"):
        sys.exit("fixture clone failed")
    assert sys.stdin.read() == "", "git consumed the installer"
    shutil.copytree(env["TEST_FIXTURE"], args[-1], ignore=shutil.ignore_patterns("tedit", ".git"))
elif name == "apt-cache":
    sys.exit(0 if args[-1] == env.get("TEST_APT_LUA", "liblua5.4-dev") else 1)
elif name in ("strip", "xcode-select"):
    pass
elif name == "install":
    sys.exit("fixture file copy failed")
elif name in ("dpkg-query", "rpm", "xbps-query"):
    sys.exit(0 if env.get("TEST_PACKAGE_OWNED") else 1)
else:
    if name == "pacman" and "-Qo" in args or name == "apk" and "info" in args or name == "pkg" and "which" in args:
        sys.exit(0 if env.get("TEST_PACKAGE_OWNED") else 1)
    phase = "update" if args[-1] == "update" else "install"
    if env.get("TEST_PM_FAIL") in (phase, "always"):
        sys.exit("fixture package manager failed: " + phase)
    assert sys.stdin.read() == "", "package manager consumed the installer"
    if phase == "install":
        state.touch()
        if "git" in args and env.get("TEST_ADD_GIT"):
            pathlib.Path(env["TEST_BIN"], "git").symlink_to(env["TEST_STUB"])
'''


class ScriptsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tedit-scripts-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / "tools"
        self.home = self.root / "home"
        self.cwd = self.root / "elsewhere"
        self.tmp = self.root / "tmp"
        self.source = self.root / "source with 'quotes"
        self.prefix = self.root / "install prefix"
        self.wrappers = self.root / "wrappers"
        for folder in (self.bin, self.home, self.cwd, self.tmp,
                       self.source / "src", self.source / "scripts", self.source / "mandoc"):
            folder.mkdir(parents=True, exist_ok=True)
        for script in (REPO / "scripts").glob("*.sh"):
            shutil.copy2(script, self.source / "scripts" / script.name)
        self.installer = self.root / "downloaded-install.sh"
        shutil.copy2(REPO / "scripts/install.sh", self.installer)
        (self.source / "src/tedit.cpp").write_text("// fixture source\n")
        (self.source / "Makefile").write_text("# fixture tedit build\n")
        (self.source / "mandoc/tedit.1").write_text("fixture manual\n")
        (self.source / ".gitignore").write_text("/tedit\n/src/*.o\n")
        self.stub = self.root / "stub"
        self.stub.write_text(f"#!{sys.executable}\n" + STUB)
        self.stub.chmod(0o755)
        for name in (*MANAGERS, "id", "uname", "sudo", "doas", "c++", "g++", "clang++",
                     "pkg-config", "pkgconf", "make", "gmake", "git", "apt-cache", "strip",
                     "xcode-select", "dpkg-query"):
            (self.bin / name).symlink_to(self.stub)
        for name in ("sh", "cat", "mkdir", "rm", "mktemp", "dirname", "sed", "grep",
                     "install", "mv", "awk", "cmp", "stat", "readlink", "cp", "chmod"):
            command = shutil.which(name)
            if command:
                (self.bin / name).symlink_to(command)
        self.release = self.root / "os-release"
        self.release.write_text('ID=debian\nID_LIKE=""\n')
        self.calls_file = self.root / "calls.jsonl"
        self.env = {
            "PATH": str(self.bin), "HOME": str(self.home), "SHELL": "/bin/sh",
            "TMPDIR": str(self.tmp), "LOG_FILE": str(self.root / "script.log"),
            "XDG_DATA_HOME": str(self.root / "data"), "TEDIT_OS_RELEASE": str(self.release),
            "TEST_STATE": str(self.root / "deps-installed"), "TEST_CALLS": str(self.calls_file),
            "TEST_BIN": str(self.bin), "TEST_STUB": str(self.stub), "TEST_FIXTURE": str(self.source),
            "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
        }

    def calls(self, name=None):
        rows = [json.loads(row) for row in self.calls_file.read_text().splitlines()] if self.calls_file.exists() else []
        return [row for row in rows if name is None or row[0] == name]

    def invoke(self, script="install", *options, pipe=False, local=True, path=False, strip=False, env=None):
        process_env = self.env | (env or {})
        if script == "install":
            argv = ["--prefix", str(self.prefix)]
            if local:
                argv += ["--repo", str(self.source)]
            if not path:
                argv += ["--no-path"]
            if not strip:
                argv += ["--no-strip"]
            command = [SH, "-s", "--"] if pipe else [SH, str(self.installer)]
        elif script == "init":
            command = [SH, str(self.source / "scripts/init.sh")]
            argv = ["--user", "--bin-dir", str(self.wrappers)]
            if not path:
                argv += ["--no-path"]
        else:
            command = [SH, str(self.source / "scripts" / f"{script}.sh")]
            argv = ["--prefix", str(self.prefix)]
            if script == "update":
                if not path:
                    argv += ["--no-path"]
                if not strip:
                    argv += ["--no-strip"]
        return subprocess.run(command + argv + list(options), cwd=self.cwd, env=process_env,
                              input=self.installer.read_text() if pipe else "", text=True,
                              capture_output=True, timeout=30)

    def ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def failed(self, result, message):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout + result.stderr)

    def test_distro_package_names_and_native_detection(self):
        cases = [
            ("debian", "Linux", "apt-get", {"build-essential", "pkg-config", "liblua5.4-dev"}),
            ("ubuntu", "Linux", "apt-get", {"build-essential", "liblua5.4-dev"}),
            ("fedora", "Linux", "dnf", {"make", "gcc-c++", "pkgconf-pkg-config", "lua-devel"}),
            ("arch", "Linux", "pacman", {"base-devel", "pkgconf", "lua"}),
            ("opensuse-tumbleweed", "Linux", "zypper", {"make", "gcc-c++", "pkgconf-pkg-config", "lua54-devel"}),
            ("alpine", "Linux", "apk", {"build-base", "pkgconf", "lua5.4-dev"}),
            ("chimera", "Linux", "apk", {"base-devel", "gmake", "pkgconf", "lua5.4-devel"}),
            ("void", "Linux", "xbps-install", {"base-devel", "pkgconf", "lua54-devel"}),
            ("solus", "Linux", "eopkg", {"pkgconf", "lua-devel"}),
            ("gentoo", "Linux", "emerge", {"dev-build/make", "sys-devel/gcc", "dev-util/pkgconf", "dev-lang/lua:5.4"}),
            ("", "FreeBSD", "pkg", {"gmake", "pkgconf", "lua54"}),
            ("", "OpenBSD", "pkg_add", {"gmake", "pkgconf", "lua%5.4"}),
            ("", "NetBSD", "pkgin", {"gmake", "pkgconf", "lua54"}),
            ("", "Darwin", "brew", {"make", "pkgconf", "lua"}),
        ]
        for distro, platform, manager, packages in cases:
            with self.subTest(distro=distro, platform=platform):
                Path(self.env["TEST_STATE"]).unlink(missing_ok=True)
                self.calls_file.unlink(missing_ok=True)
                self.release.write_text(f'ID="{distro}"\nID_LIKE=""\n')
                self.ok(self.invoke("install", "--deps-only", local=False,
                                    env={"TEST_OS": platform, "TEST_MISSING_BUILD": "1", "TEST_UID": "1000"}))
                calls = [call for call in self.calls() if call[0] in MANAGERS]
                self.assertEqual({call[0] for call in calls}, {manager})
                self.assertTrue(any(packages <= set(call[1]) for call in calls), calls)
                if manager == "eopkg":
                    self.assertIn(["install", "-y", "-c", "system.devel"], [call[1] for call in calls])
                if manager == "pacman":
                    self.assertTrue(all("-Sy" not in call[1] for call in calls))
                self.assertFalse(self.calls("git"))
                self.assertFalse(any("all" in call[1] for call in self.calls("gmake")))

    def test_id_like_is_an_ordered_list(self):
        self.release.write_text('ID=custom\nID_LIKE="unknown ubuntu debian"\n')
        self.ok(self.invoke("install", "--deps-only", env={"TEST_MISSING_BUILD": "1"}))
        self.assertTrue(self.calls("apt-get"))
        self.assertFalse(self.calls("dnf"))

    def test_yum_and_macports_fallbacks(self):
        for distro, platform, absent, manager, package in (
                ("rhel", "Linux", "dnf", "yum", "lua-devel"),
                ("", "Darwin", "brew", "port", "lua54")):
            with self.subTest(manager=manager):
                (self.bin / absent).unlink()
                self.release.write_text(f'ID="{distro}"\n')
                Path(self.env["TEST_STATE"]).unlink(missing_ok=True)
                self.calls_file.unlink(missing_ok=True)
                self.ok(self.invoke("install", "--deps-only", env={"TEST_OS": platform, "TEST_MISSING_BUILD": "1"}))
                self.assertIn(package, self.calls(manager)[-1][1])

    def test_apt_selects_available_lua(self):
        self.ok(self.invoke("install", "--deps-only", env={"TEST_MISSING_BUILD": "1", "TEST_APT_LUA": "liblua5.3-dev"}))
        self.assertIn("liblua5.3-dev", self.calls("apt-get")[-1][1])

    def test_failures_do_not_report_dependency_success(self):
        for phase in ("update", "install"):
            with self.subTest(phase=phase):
                Path(self.env["TEST_STATE"]).unlink(missing_ok=True)
                result = self.invoke("install", "--deps-only", env={"TEST_MISSING_BUILD": "1", "TEST_PM_FAIL": phase})
                self.failed(result, "fixture package manager failed")
                self.assertNotIn("Dependencies verified", result.stdout)
                self.assertFalse(self.calls("git"))

    def test_successful_packages_are_rechecked(self):
        self.failed(self.invoke("install", "--deps-only", env={"TEST_MISSING_BUILD": "1", "TEST_STILL_BROKEN": "1"}), "still unusable")

    def test_no_deps_never_calls_a_package_manager(self):
        self.failed(self.invoke("install", "--no-deps", env={"TEST_MISSING_BUILD": "1"}), "--no-deps")
        self.assertFalse([row for row in self.calls() if row[0] in MANAGERS])

    def test_piped_installer_fetches_source_and_cleans_up(self):
        self.ok(self.invoke("install", "--ref", "v2-test", pipe=True, local=False))
        clone = self.calls("git")[0][1]
        self.assertEqual(clone[:5], ["clone", "--depth", "1", "--branch", "v2-test"])
        self.assertTrue((self.prefix / "bin/tedit").is_file())
        self.assertTrue((self.prefix / "share/man/man1/tedit.1").is_file())
        self.assertEqual(list(self.tmp.iterdir()), [])

    def test_downloaded_installer_bootstraps_missing_git(self):
        (self.bin / "git").unlink()
        self.ok(self.invoke("install", local=False, env={"TEST_ADD_GIT": "1"}))
        self.assertIn("git", self.calls("apt-get")[-1][1])
        self.assertTrue(self.calls("git"))

    def test_clone_and_build_failures_stop_installation(self):
        for failure in ("TEST_CLONE_FAIL", "TEST_BUILD_FAIL"):
            with self.subTest(failure=failure):
                self.failed(self.invoke("install", local=False, env={failure: "1"}), "fixture")
                self.assertFalse((self.prefix / "bin/tedit").exists())
                self.assertEqual(list(self.tmp.iterdir()), [])

    def test_lua_flags_and_filesystem_library_reach_make(self):
        self.ok(self.invoke("install", "--no-deps", env={"TEST_BAD_LUA_FIRST": "1", "TEST_FS_LIBRARY": "1"}))
        build = [row[1] for row in self.calls("gmake") if "all" in row[1]][0]
        self.assertIn("LUA_LIBS=-llua_fixture", build)
        self.assertIn("LDFLAGS= -lstdc++fs", build)

    @unittest.skipUnless(Path("/usr/include/lua.h").exists(), "system has no unversioned Lua headers")
    def test_unversioned_system_headers_without_pkg_config(self):
        self.ok(self.invoke("install", "--no-deps", env={"TEST_PC_EMPTY": "1", "TEST_LUA_LIBRARY": "-llua"}))
        self.assertFalse([row for row in self.calls() if row[0] in MANAGERS])

    def test_flags_prevent_man_strip_and_installed_changes(self):
        self.ok(self.invoke("install", "--no-man"))
        self.assertFalse((self.prefix / "share/man").exists())
        self.assertFalse(self.calls("strip"))
        installed = self.prefix / "bin/tedit"
        installed.write_text("old installation\n")
        self.ok(self.invoke("install", "--no-install", strip=True))
        self.assertEqual(installed.read_text(), "old installation\n")
        self.assertFalse(self.calls("strip"))
        self.assertFalse((self.home / ".profile").exists())

    def test_existing_build_can_install_without_compiler(self):
        binary = self.source / "tedit"
        binary.write_text("#!/bin/sh\nexit 0\n")
        binary.chmod(0o755)
        self.ok(self.invoke("install", "--no-build", "--no-deps", env={"TEST_MISSING_BUILD": "1"}))
        self.assertFalse(self.calls("c++"))

    def test_failed_file_copy_preserves_previous_installation(self):
        self.ok(self.invoke("install"))
        binary = self.prefix / "bin/tedit"
        binary.write_text("old installed contents\n")
        (self.bin / "install").unlink()
        (self.bin / "install").symlink_to(self.stub)
        self.failed(self.invoke("install"), "fixture file copy failed")
        self.assertEqual(binary.read_text(), "old installed contents\n")
        self.assertFalse(list((self.prefix / "bin").glob(".tedit.*")))

    def test_user_prefix_and_path_quoting_are_idempotent(self):
        self.prefix = self.root / "prefix's $(touch INJECTED)"
        for _ in range(2):
            self.ok(self.invoke("install", path=True, env={"TEST_UID": "1000"}))
        profile = self.home / ".profile"
        self.assertEqual(profile.read_text().count("# Added by tedit installer"), 1)
        check = subprocess.run([SH, "-c", '. "$1"; printf "%s" "$PATH"', "sh", str(profile)],
                               env=self.env, cwd=self.cwd, text=True, capture_output=True)
        self.ok(check)
        self.assertTrue(check.stdout.startswith(str(self.prefix / "bin") + ":"))
        self.assertFalse((self.cwd / "INJECTED").exists())
        self.assertFalse(self.calls("sudo"))

    def test_privilege_failure_stops_and_doas_is_used_consistently(self):
        self.failed(self.invoke("install", "--deps-only", env={"TEST_UID": "1000", "TEST_MISSING_BUILD": "1", "TEST_AUTH_FAIL": "1"}), "authentication failed")
        self.assertFalse(self.calls("apt-get"))
        (self.bin / "sudo").unlink()
        self.ok(self.invoke("install", "--deps-only", env={"TEST_UID": "1000", "TEST_MISSING_BUILD": "1"}))
        self.assertTrue(all(row[2] == "1" for row in self.calls("apt-get")))

    def test_all_command_help_and_missing_values(self):
        for script in ("install", "init", "update", "uninstall"):
            with self.subTest(script=script):
                self.ok(self.invoke(script, "--help"))
                option = "--bin-dir" if script == "init" else "--prefix"
                self.failed(self.invoke(script, option), "needs a path" if script != "install" else "needs a value")

    def test_wrappers_work_from_any_directory_and_preserve_arguments(self):
        self.ok(self.invoke("init"))
        wrapper = self.wrappers / "tedit-install"
        result = subprocess.run([str(wrapper), "--prefix", str(self.prefix), "--no-deps", "--no-path", "--no-strip"],
                                cwd=self.cwd, env=self.env, text=True, capture_output=True, timeout=30)
        self.ok(result)
        self.assertTrue((self.prefix / "bin/tedit").exists())
        self.ok(self.invoke("init"))

    def test_init_refuses_foreign_files_before_writing_markers(self):
        self.wrappers.mkdir()
        foreign = self.wrappers / "tedit-update"
        foreign.write_text("user command\n")
        self.failed(self.invoke("init"), "Refusing to overwrite")
        self.assertFalse((self.wrappers / "tedit-install").exists())
        self.assertFalse((self.root / "data/tedit/repo").exists())
        self.ok(self.invoke("init", "--force"))

    def test_init_show_never_authenticates_or_installs(self):
        result = self.invoke("init", "--show", "--system")
        self.ok(result)
        self.assertIn("MODE=system", result.stdout)
        self.assertFalse(self.calls("sudo"))
        self.assertFalse(self.wrappers.exists())

    def test_init_force_still_refuses_directories(self):
        (self.wrappers / "tedit-install").mkdir(parents=True)
        self.failed(self.invoke("init", "--force"), "Refusing to replace a directory")
        self.assertEqual(list((self.wrappers / "tedit-install").iterdir()), [])

    def test_uninstall_dry_run_and_profile_preservation(self):
        self.ok(self.invoke("install", path=True, env={"TEST_UID": "1000"}))
        profile = self.home / ".profile"
        with profile.open("a") as stream:
            stream.write("KEEP_THIS_LINE=1\n")
        profile.chmod(0o600)
        original = profile.read_bytes()
        self.ok(self.invoke("uninstall", "--dry-run"))
        self.assertEqual(profile.read_bytes(), original)
        self.assertTrue((self.prefix / "bin/tedit").exists())
        self.ok(self.invoke("uninstall"))
        self.assertEqual(profile.read_text().strip(), "KEEP_THIS_LINE=1")
        self.assertEqual(profile.stat().st_mode & 0o777, 0o600)
        self.assertFalse((self.prefix / "bin/tedit").exists())
        self.assertTrue((self.source / "tedit").exists())

    def test_profile_symlink_and_unrelated_path_blocks_survive(self):
        target = self.home / "shell-config"
        target.write_text("# Added by tedit init\nexport PATH=\"/other/bin:$PATH\"\nKEEP=1\n")
        (self.home / ".profile").symlink_to(target)
        self.ok(self.invoke("uninstall"))
        self.assertTrue((self.home / ".profile").is_symlink())
        self.assertIn("/other/bin", target.read_text())

    def test_cleaning_path_preserves_profile_symlinks(self):
        self.ok(self.invoke("install", path=True, env={"TEST_UID": "1000"}))
        profile = self.home / ".profile"
        target = self.home / "shell-config"
        profile.rename(target)
        profile.symlink_to(target)
        with target.open("a") as stream:
            stream.write("KEEP=1\n")
        self.ok(self.invoke("uninstall"))
        self.assertTrue(profile.is_symlink())
        self.assertEqual(target.read_text().strip(), "KEEP=1")

    def test_uninstall_removes_only_selected_installation(self):
        self.ok(self.invoke("install"))
        other = self.root / "another/bin/tedit"
        other.parent.mkdir(parents=True)
        other.write_text("keep\n")
        self.ok(self.invoke("uninstall", env={"PATH": str(other.parent) + ":" + self.env["PATH"]}))
        self.assertEqual(other.read_text(), "keep\n")

    def test_package_owned_files_are_not_silently_removed(self):
        self.ok(self.invoke("install"))
        self.failed(self.invoke("uninstall", env={"TEST_PACKAGE_OWNED": "1"}), "package-owned")
        self.assertTrue((self.prefix / "bin/tedit").exists())
        self.ok(self.invoke("uninstall", "--force", env={"TEST_PACKAGE_OWNED": "1"}))

    def test_repo_purge_needs_yes_and_dry_run_keeps_everything(self):
        (self.source / ".git").mkdir()
        self.ok(self.invoke("install"))
        self.failed(self.invoke("uninstall", "--purge-repo"), "needs --yes")
        self.assertTrue((self.prefix / "bin/tedit").exists())
        self.ok(self.invoke("uninstall", "--purge-repo", "--dry-run"))
        self.assertTrue(self.source.exists())
        self.ok(self.invoke("uninstall", "--purge-repo", "--yes"))
        self.assertFalse(self.source.exists())

    def git(self, *args, cwd=None):
        result = subprocess.run([GIT, *args], cwd=cwd or self.source, env=self.env,
                                text=True, capture_output=True, timeout=15)
        self.ok(result)
        return result.stdout.strip()

    def init_git(self):
        if not GIT:
            self.skipTest("git is required")
        (self.bin / "git").unlink()
        (self.bin / "git").symlink_to(GIT)
        self.remote = self.root / "remote.git"
        self.git("init", "--bare", "--initial-branch=main", str(self.remote), cwd=self.cwd)
        self.git("init", "--initial-branch=main")
        self.git("add", ".")
        self.git("-c", "user.name=Tests", "-c", "user.email=tests@example.invalid", "commit", "-m", "Initial")
        self.git("remote", "add", "origin", str(self.remote))
        self.git("push", "-u", "origin", "main")

    def incoming_commit(self):
        donor = self.root / "donor"
        self.git("clone", str(self.remote), str(donor), cwd=self.cwd)
        (donor / "src/tedit.cpp").write_text("// updated source\n")
        self.git("add", ".", cwd=donor)
        self.git("-c", "user.name=Tests", "-c", "user.email=tests@example.invalid", "commit", "-m", "Update", cwd=donor)
        self.git("push", cwd=donor)

    def test_updater_delegates_no_install_and_dependency_flags(self):
        self.ok(self.invoke("update", "--no-git", "--no-install", "--no-man", "--no-deps"))
        self.assertTrue((self.source / "tedit").exists())
        self.assertFalse(self.prefix.exists())
        self.assertFalse([row for row in self.calls() if row[0] in MANAGERS])

    def test_updater_fast_forwards_and_builds_new_source(self):
        self.init_git()
        self.incoming_commit()
        self.ok(self.invoke("update", "--no-deps"))
        self.assertEqual((self.source / "src/tedit.cpp").read_text(), "// updated source\n")
        self.assertTrue((self.prefix / "bin/tedit").exists())

    def test_updater_dirty_checkout_is_preserved_on_incoming_changes(self):
        self.init_git()
        self.incoming_commit()
        (self.source / "src/tedit.cpp").write_text("// local changes\n")
        head = self.git("rev-parse", "HEAD")
        self.failed(self.invoke("update"), "Local changes")
        self.assertEqual(self.git("rev-parse", "HEAD"), head)
        self.assertEqual((self.source / "src/tedit.cpp").read_text(), "// local changes\n")
        self.assertEqual(self.git("stash", "list"), "")

    def test_updater_fetch_failure_never_builds(self):
        self.init_git()
        self.git("remote", "set-url", "origin", str(self.root / "missing.git"))
        self.failed(self.invoke("update"), "Git fetch failed")
        self.assertFalse((self.source / "tedit").exists())

    def test_updater_divergence_does_not_merge_or_rebase(self):
        self.init_git()
        self.incoming_commit()
        (self.source / "local-file").write_text("local\n")
        self.git("add", ".")
        self.git("-c", "user.name=Tests", "-c", "user.email=tests@example.invalid", "commit", "-m", "Local")
        head = self.git("rev-parse", "HEAD")
        self.failed(self.invoke("update"), "history diverged")
        self.assertEqual(self.git("rev-parse", "HEAD"), head)

    def test_updater_up_to_date_does_not_rebuild(self):
        self.init_git()
        self.ok(self.invoke("install"))
        self.calls_file.unlink()
        result = self.invoke("update")
        self.ok(result)
        self.assertIn("Up to date.", result.stdout)
        self.assertFalse(self.calls("gmake"))

    def test_failed_update_is_retried_even_when_source_is_current(self):
        self.init_git()
        self.ok(self.invoke("install"))
        old_receipt = (self.prefix / "share/tedit/build-revision").read_text()
        self.incoming_commit()
        self.failed(self.invoke("update", env={"TEST_BUILD_FAIL": "1"}), "fixture build failed")
        self.assertEqual((self.prefix / "share/tedit/build-revision").read_text(), old_receipt)
        self.calls_file.unlink()
        self.ok(self.invoke("update"))
        self.assertTrue(any("all" in row[1] for row in self.calls("gmake")))
        self.assertEqual((self.prefix / "share/tedit/build-revision").read_text().strip(), self.git("rev-parse", "HEAD"))

    def test_source_only_update_never_changes_an_installed_binary(self):
        self.init_git()
        self.incoming_commit()
        self.ok(self.invoke("update", "--no-build", "--no-install"))
        self.assertFalse(self.calls("gmake"))
        self.assertFalse(self.prefix.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
