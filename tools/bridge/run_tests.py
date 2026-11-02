"""Run the bridge tests, then every mutation twin. Prints ALL OK as the last line only when everything passes and every twin is caught.

    python -I tools/bridge/run_tests.py [--no-cpp]

Steps:
  1. test_bridge.py: the Python client against the fake game (fake_game.py).
  2. Python twins: a copy of bridge_client.py with one guard broken (timeouts ignored, errors swallowed, replies matched to
     the wrong call, ...). The test that guards it must fail against the copy.
  3. C++: build cpp/test_server.cpp (it embeds include/coopbridge/server.h) with cmake, one job at a time, then run
     test_cpp_server.py against it: the Python client talking to the C++ server over TCP and a named pipe (Windows).
  4. C++ twins: a copy of the headers with one guard broken (calls run off the main thread, no per-frame budget, errors
     swallowed, no timeout, ...), built into its own test program. The test that guards it must fail against that program.

The build goes to tools/bridge/build/ (ignored by git) or to the folder in COOPBRIDGE_BUILD_DIR. CMAKE_TOOLCHAIN_FILE and the other
generator variables are removed from the environment for the build, because a toolchain left over from another target (an
Android NDK, say) would send the build to the wrong compiler. If cmake or a C++ compiler is missing, step 3 and 4 print SKIP and the
last line says so instead of ALL OK.
No test starts a real game.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

# name -> (file, text that must occur exactly once, replacement, the test that must fail)
PY_TWINS = {
    "timeout_ignored": (
        "bridge_client.py",
        "            if left <= 0:\n                return None\n",
        "            if False:\n                return None\n",
        "ErrorTests.test_no_reply_in_time_is_a_timeout_and_the_late_reply_is_skipped"),
    "error_swallowed": (
        "bridge_client.py",
        '            raise RpcError(int(e.get("code", 0) or 0), str(e.get("message", "")), method, e.get("data"))\n',
        "            return None\n",
        "ErrorTests.test_game_errors_become_rpc_errors"),
    "id_not_checked": (
        "bridge_client.py",
        "            if mid != rid:\n",
        "            if False:\n",
        "ErrorTests.test_a_reply_with_the_wrong_id_is_a_protocol_error"),
    "stale_reply_not_skipped": (
        "bridge_client.py",
        "            if isinstance(mid, int) and not isinstance(mid, bool) and mid < rid:\n",
        "            if False:\n",
        "ErrorTests.test_no_reply_in_time_is_a_timeout_and_the_late_reply_is_skipped"),
    "partial_line_not_buffered": (
        "bridge_client.py",
        '        while b"\\n" not in self._buf:\n',
        '        if b"\\n" not in self._buf:\n',
        "ErrorTests.test_a_reply_that_arrives_in_two_pieces"),
    "connect_does_not_retry": (
        "bridge_client.py",
        "                if time.monotonic() >= deadline:\n                    raise\n                time.sleep(0.25)\n",
        "                raise\n",
        "ConnectTests.test_connect_waits_for_the_game_to_start"),
    "crash_not_detected": (
        "bridge_client.py",
        "if self.game_pid and not pid_alive(self.game_pid):",
        "if False:",
        "ErrorTests.test_dropped_connection_of_a_dead_game_is_a_crash"),
    "every_drop_is_a_crash": (
        "bridge_client.py",
        "if self.game_pid and not pid_alive(self.game_pid):",
        "if self.game_pid:",
        "ErrorTests.test_dropped_connection_is_connection_lost"),
    "wait_for_never_raises": (
        "bridge_client.py",
        '        if not r.get("satisfied"):\n            raise WaitTimeout(expr or cmd or "", r.get("value"))\n',
        '        if False:\n            raise WaitTimeout(expr or cmd or "", r.get("value"))\n',
        "StandardMethodTests.test_wait_for_that_never_holds_raises"),
    "event_cursor_ignored": (
        "bridge_client.py",
        "        cur = since\n",
        "        cur = 0\n",
        "StandardMethodTests.test_wait_event_respects_the_cursor"),
    "relative_path_sent_as_is": (
        "bridge_client.py",
        '        p: Dict[str, Any] = {"path": os.path.abspath(path)}\n        if sections:',
        '        p: Dict[str, Any] = {"path": path}\n        if sections:',
        "StandardMethodTests.test_state_to_file_makes_the_path_absolute"),
    "protocol_not_checked": (
        "bridge_client.py",
        "        if not isinstance(proto, int) or proto > PROTOCOL_VERSION:\n",
        "        if False:\n",
        "StandardMethodTests.test_a_game_with_a_newer_protocol_is_refused_with_a_clear_message"),
    "no_call_lock": (
        "bridge_client.py",
        "        with self._lock:\n            rid = next(self._ids)\n",
        "        if True:\n            rid = next(self._ids)\n",
        "ErrorTests.test_calls_from_two_threads_do_not_mix_replies"),
}

# name -> (text that must occur exactly once in include/coopbridge/server.h, replacement, the test that must fail)
CPP_TWINS = {
    "run_off_main_thread": (
        "            inbox_.push_back(job);\n",
        "            Dispatch(job);\n",
        "ServerTests.test_calls_run_on_the_main_thread"),
    "no_queue_limit": (
        "if (inbox_.size() >= opt_.maxPending) {",
        "if (false) {",
        "ServerTests.test_queue_limit_answers_busy"),
    "no_per_pump_budget": (
        "while (!inbox_.empty() && take.size() < opt_.maxCallsPerPump) {",
        "while (!inbox_.empty()) {",
        "ServerTests.test_requests_per_pump_are_capped"),
    "main_thread_timeout_ignored": (
        "bool finished = job->cv.wait_for(lk, std::chrono::milliseconds(hardMs), [&] { return job->done; });",
        "job->cv.wait(lk, [&] { return job->done; }); bool finished = true; (void)hardMs;",
        "ServerTests.test_main_thread_timeout_and_abandoned_request"),
    "adapter_error_swallowed": (
        "job.FinishErr(errc::kHost, e.what());",
        "job.FinishOk(json::Value::object()); (void)e;",
        "ServerTests.test_errors_keep_their_code_and_message"),
    "reply_with_wrong_id": (
        'resp.set("id", id);',
        'resp.set("id", id.isNumber() ? json::Value(id.asInt() + 1) : id);',
        "ServerTests.test_framing_and_bad_requests"),
    "events_cursor_stuck": (
        "next = e.seq;",
        "next = since;",
        "ServerTests.test_events_cursor_and_overflow"),
    "listens_on_any_address": (
        "if (!opt_.allowRemote && !IsLoopback(opt_.tcpHost)) {",
        "if (false) {",
        "StartStopTests.test_non_loopback_host_is_refused"),
    "one_request_per_read": (
        "while ((nl = buf.find('\\n')) != std::string::npos) {",
        "if ((nl = buf.find('\\n')) != std::string::npos) {",
        "ServerTests.test_framing_and_bad_requests"),
    "wait_for_has_no_deadline": (
        "if (now >= job.deadlineMs) {",
        "if (false) {",
        "ServerTests.test_wait_for_timeout_is_a_result_not_a_hang"),
    "unknown_method_succeeds": (
        'return Reply(id, false, ErrorObj(errc::kMethodNotFound, "unknown method: " + method));',
        "return Reply(id, true, json::Value());",
        "ServerTests.test_errors_keep_their_code_and_message"),
    "save_has_no_timeout": (
        'if (NowMs() >= j.deadlineMs) { j.FinishErr(errc::kTimeout, "timed out waiting for the game to finish saving"); return true; }',
        "",
        "ServerTests.test_save_that_never_finishes_times_out_with_its_own_code"),
}


def host_env():
    env = dict(os.environ)
    for k in ("CMAKE_TOOLCHAIN_FILE", "CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET"):
        env.pop(k, None)
    return env


def unittest_run(script, test=None, env_extra=None):
    """Run a unittest file (optionally one test) in a child. Returns (exit code, combined output, tests run)."""
    env = dict(os.environ, **(env_extra or {}))
    args = [sys.executable, "-I", str(HERE / script)] + ([test] if test else [])
    p = subprocess.run(args, cwd=HERE, env=env, capture_output=True, text=True, errors="replace", timeout=600)
    out = (p.stdout or "") + (p.stderr or "")
    m = re.search(r"Ran (\d+) tests?", out)
    return p.returncode, out, int(m.group(1)) if m else 0


def caught(rc, out):
    """A twin counts as caught only when a test failed. An import error or a broken fixture is not a catch."""
    return rc != 0 and re.search(r"Ran \d+ tests? in", out) is not None and "setUpModule" not in out and "ImportError" not in out \
        and "SyntaxError" not in out and re.search(r"(FAIL|ERROR): test_", out) is not None


def replace_once(path, old, new):
    text = path.read_text(encoding="utf-8")
    n = text.count(old)
    if n != 1:
        raise SystemExit(f"twin setup is stale: {old.strip()[:60]!r} occurs {n} times in {path.name}, expected once")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="")


def python_stage():
    print("== Python client against the fake game")
    rc, out, n = unittest_run("test_bridge.py")
    print("\n".join(out.strip().splitlines()[-4:]))
    if rc != 0:
        print(out[-4000:])
        return [], 0, 0, ["test_bridge"]
    bad = []
    done = 0
    tmp = Path(tempfile.mkdtemp(prefix="coopbridge-twins-"))
    try:
        for name, (fname, old, new, test) in PY_TWINS.items():
            d = tmp / name
            d.mkdir()
            shutil.copy(HERE / fname, d / fname)
            replace_once(d / fname, old, new)
            rc, out, _ = unittest_run("test_bridge.py", test, {"COOPBRIDGE_CLIENT_DIR": str(d)})
            ok = caught(rc, out)
            print(f"twin {name:26s} via {test.split('.')[-1][:60]}: {'caught' if ok else 'NOT CAUGHT'}")
            if ok:
                done += 1
            else:
                bad.append(name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return bad, n, done, []


def find_exe(build, name):
    for pat in (name + ".exe", name):
        for p in build.rglob(pat):
            if p.is_file():
                return p
    return None


def build_cpp(mut_dir):
    """Returns (build dir, None) or (None, reason it was skipped). A build that fails is an error, not a skip."""
    cmake = shutil.which("cmake")
    if not cmake:
        return None, "cmake not found on PATH"
    build = Path(os.environ.get("COOPBRIDGE_BUILD_DIR") or (HERE / "build"))
    build.mkdir(parents=True, exist_ok=True)
    cfg = subprocess.run([cmake, "-S", str(HERE / "cpp"), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
                          "-DCOOPBRIDGE_MUTANTS=" + ";".join(CPP_TWINS), "-DCOOPBRIDGE_MUT_DIR=" + str(mut_dir)],
                         env=host_env(), capture_output=True, text=True, errors="replace")
    if cfg.returncode != 0:
        return None, "cmake could not configure (no C++ compiler?): " + " ".join((cfg.stdout + cfg.stderr).split())[-200:]
    b = subprocess.run([cmake, "--build", str(build), "--config", "Release", "--parallel", "1"],
                       env=host_env(), capture_output=True, text=True, errors="replace")
    if b.returncode != 0:
        print((b.stdout + b.stderr)[-4000:])
        return None, "C++ build FAILED"
    return build, None


def cpp_stage():
    """Returns (problems, skipped reason or None, tests run, twins caught)."""
    print("== C++ server template against the Python client")
    mut = Path(tempfile.mkdtemp(prefix="coopbridge-mut-"))
    try:
        for name, (old, new, _test) in CPP_TWINS.items():
            d = mut / name / "coopbridge"
            d.mkdir(parents=True)
            shutil.copy(HERE / "include" / "coopbridge" / "json.h", d / "json.h")
            shutil.copy(HERE / "include" / "coopbridge" / "server.h", d / "server.h")
            replace_once(d / "server.h", old, new)
        build, why = build_cpp(mut)
        if why:
            if why == "C++ build FAILED":
                return ["cpp_build"], None, 0, 0
            print("SKIP: C++ stage not run:", why)
            return [], why, 0, 0
        exe = find_exe(build, "coopbridge_test")
        if not exe:
            return ["cpp_exe_missing"], None, 0, 0
        rc, out, n = unittest_run("test_cpp_server.py", None, {"COOPBRIDGE_TEST_EXE": str(exe)})
        print("\n".join(out.strip().splitlines()[-4:]))
        if rc != 0:
            print(out[-4000:])
            return ["test_cpp_server"], None, n, 0
        bad = []
        done = 0
        for name, (_old, _new, test) in CPP_TWINS.items():
            m = find_exe(build, "coopbridge_test_" + name)
            if not m:
                bad.append(name)
                print(f"twin {name}: program missing")
                continue
            rc, out, _ = unittest_run("test_cpp_server.py", test, {"COOPBRIDGE_TEST_EXE": str(m)})
            ok = caught(rc, out)
            print(f"twin {name:28s} via {test.split('.')[-1][:56]}: {'caught' if ok else 'NOT CAUGHT'}")
            if ok:
                done += 1
            else:
                bad.append(name)
        return bad, None, n, done
    finally:
        shutil.rmtree(mut, ignore_errors=True)


def main():
    bad, n_py, caught_py, early = python_stage()
    if early:
        print("RESULT: FAIL (real tests):", early)
        return 1
    skipped = None
    n_cpp = caught_cpp = 0
    if "--no-cpp" in sys.argv:
        skipped = "--no-cpp given"
        print("SKIP: C++ stage not run: --no-cpp given")
    else:
        cpp_bad, skipped, n_cpp, caught_cpp = cpp_stage()
        bad += cpp_bad
    if bad:
        print("RESULT: FAIL, problems:", bad)
        return 1
    print(f"tests: {n_py} client tests, {n_cpp} C++ server tests; "
          f"twins caught: {caught_py} of {len(PY_TWINS)} client, {caught_cpp} of {len(CPP_TWINS)} C++ server")
    print("ALL OK" + (f" (C++ stage skipped: {skipped})" if skipped else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
