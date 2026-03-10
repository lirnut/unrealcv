"""
UnrealCV Debug Harness - Test Runner
Handles game launch, server detection, and test execution.
"""
import os
import socket
import subprocess
import sys
import time
import threading
from pathlib import Path
from typing import Optional, List, Callable, Dict
from dataclasses import dataclass, field
from enum import Enum
import json

# Add plugin Source path for unrealcv import
WORKFLOW_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = WORKFLOW_DIR.parent
CLIENT_PYTHON_DIR = PLUGIN_ROOT / "Source" / "uezoo"
if CLIENT_PYTHON_DIR.exists():
    sys.path.insert(0, str(CLIENT_PYTHON_DIR))

from config import get_config
from log_monitor import LogMonitor, LogEntry, ConsoleLogPrinter


class TestStatus(Enum):
    PENDING = "pending"
    STARTING = "starting"
    WAITING_FOR_SERVER = "waiting_for_server"
    RUNNING = "running"
    PASSED = "passed"
    FAILED = "failed"
    TIMEOUT = "timeout"
    CANCELLED = "cancelled"


@dataclass
class TestResult:
    name: str
    status: TestStatus
    duration: float
    message: str = ""
    details: Dict = field(default_factory=dict)


@dataclass
class TestSuiteResult:
    overall_status: TestStatus
    total_tests: int
    passed: int
    failed: int
    duration: float
    results: List[TestResult] = field(default_factory=list)
    logs: List[str] = field(default_factory=list)


class UETestRunner:
    """UnrealCV test runner with game lifecycle management"""

    def __init__(self):
        self.config = get_config()
        self._game_proc: Optional[subprocess.Popen] = None
        self._log_monitor: Optional[LogMonitor] = None
        self._status_callbacks: List[Callable[[TestStatus, str], None]] = []
        self._cancelled = False
        self._server_ready = False

    def add_status_callback(self, callback: Callable[[TestStatus, str], None]):
        """Add status change callback"""
        self._status_callbacks.append(callback)

    def _notify_status(self, status: TestStatus, message: str = ""):
        """Notify status change"""
        for callback in self._status_callbacks:
            try:
                callback(status, message)
            except Exception as e:
                print(f"Callback error: {e}")

    def cancel(self):
        """Cancel test execution"""
        self._cancelled = True
        if self._game_proc:
            self._game_proc.terminate()

    def launch_game(self, extra_args: Optional[List[str]] = None) -> bool:
        """Launch the UE game executable"""
        config = self.config
        exe_path = config.exe_path

        if not exe_path.exists():
            print(f"[Launch] Executable not found: {exe_path}")
            return False

        env = os.environ.copy()
        env["UE-CV-PORT"] = str(config.port)

        cmd = [
            str(exe_path),
            f"-Port={config.port}",
            "-Log",
            "-NoSplash",
            "-NoPause",
            "-FullStdOutLogOutput",
            "-RenderOffScreen",  # Headless mode option
        ]

        if extra_args:
            cmd.extend(extra_args)

        print(f"\n[Launch] Starting game: {exe_path.name}")
        print(f"[Launch] Command: {' '.join(cmd)}\n")

        try:
            self._game_proc = subprocess.Popen(
                cmd,
                cwd=str(exe_path.parent),
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                encoding='utf-8',
                errors='ignore'
            )

            # Start log monitoring
            self._log_monitor = LogMonitor(buffer_size=config.log_buffer_size)
            printer = ConsoleLogPrinter(show_category=True)

            # Configure log filter
            self._log_monitor.filter.include_keywords = config.log_filter_keywords
            self._log_monitor.filter.exclude_patterns = config.log_exclude_patterns
            self._log_monitor.add_callback(printer)

            self._log_monitor.start_monitoring_process(self._game_proc)

            return True
        except Exception as e:
            print(f"[Launch] Failed to start game: {e}")
            return False

    def wait_for_server(self, timeout: Optional[int] = None) -> bool:
        """Wait for UnrealCV server to be ready"""
        if timeout is None:
            timeout = self.config.server_ready_timeout

        config = self.config
        start_time = time.time()

        self._notify_status(TestStatus.WAITING_FOR_SERVER, f"Waiting for server on port {config.port}")
        print(f"\n[Server] Waiting for UnrealCV server (timeout: {timeout}s)...")

        while time.time() - start_time < timeout:
            if self._cancelled:
                return False

            # Check if process died
            if self._game_proc and self._game_proc.poll() is not None:
                exit_code = self._game_proc.returncode
                print(f"[Server] Game exited with code {exit_code}")
                return False

            # Check TCP port
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                result = sock.connect_ex((config.host, config.port))
                sock.close()
                if result == 0:
                    elapsed = time.time() - start_time
                    print(f"[Server] Ready after {elapsed:.1f}s")
                    self._server_ready = True
                    return True
            except Exception:
                pass

            time.sleep(0.5)

        print(f"[Server] Timeout after {timeout}s")
        return False

    def run_basic_tests(self) -> TestSuiteResult:
        """Run basic connectivity and API tests"""
        import unrealcv

        config = self.config
        start_time = time.time()
        results = []

        self._notify_status(TestStatus.RUNNING, "Running basic tests")

        # Connect client
        client = unrealcv.Client((config.host, config.port))

        # Test 1: Connection
        conn_start = time.time()
        try:
            if client.connect(timeout=10):
                results.append(TestResult(
                    name="Connection",
                    status=TestStatus.PASSED,
                    duration=time.time() - conn_start,
                    message="Successfully connected to server"
                ))
            else:
                results.append(TestResult(
                    name="Connection",
                    status=TestStatus.FAILED,
                    duration=time.time() - conn_start,
                    message="Failed to connect"
                ))
                return TestSuiteResult(
                    overall_status=TestStatus.FAILED,
                    total_tests=1,
                    passed=0,
                    failed=1,
                    duration=time.time() - start_time,
                    results=results
                )
        except Exception as e:
            results.append(TestResult(
                name="Connection",
                status=TestStatus.FAILED,
                duration=time.time() - conn_start,
                message=f"Connection error: {e}"
            ))
            return TestSuiteResult(
                overall_status=TestStatus.FAILED,
                total_tests=1,
                passed=0,
                failed=1,
                duration=time.time() - start_time,
                results=results
            )

        # Define tests
        tests = [
            ("Version", "vget /unrealcv/version"),
            ("Status", "vget /unrealcv/status"),
            ("Cameras", "vget /cameras"),
            ("Camera 0 Location", "vget /camera/0/location"),
            ("Camera 0 Rotation", "vget /camera/0/rotation"),
            ("Camera 0 FOV", "vget /camera/0/fov"),
            ("Objects", "vget /objects"),
        ]

        for name, cmd in tests:
            if self._cancelled:
                results.append(TestResult(
                    name=name,
                    status=TestStatus.CANCELLED,
                    duration=0,
                    message="Test cancelled"
                ))
                break

            test_start = time.time()
            try:
                res = client.request(cmd)
                duration = time.time() - test_start

                if res and not res.startswith("error"):
                    results.append(TestResult(
                        name=name,
                        status=TestStatus.PASSED,
                        duration=duration,
                        message=f"Response: {res[:100]}"
                    ))
                else:
                    results.append(TestResult(
                        name=name,
                        status=TestStatus.FAILED,
                        duration=duration,
                        message=f"Error: {res}"
                    ))
            except Exception as e:
                duration = time.time() - test_start
                results.append(TestResult(
                    name=name,
                    status=TestStatus.FAILED,
                    duration=duration,
                    message=f"Exception: {e}"
                ))

        client.disconnect()

        # Calculate summary
        passed = sum(1 for r in results if r.status == TestStatus.PASSED)
        failed = sum(1 for r in results if r.status == TestStatus.FAILED)
        total_duration = time.time() - start_time

        overall = TestStatus.PASSED if failed == 0 else TestStatus.FAILED

        # Get recent errors from log monitor
        logs = []
        if self._log_monitor:
            errors = self._log_monitor.get_errors()
            logs = [str(e) for e in errors[-10:]]  # Last 10 errors

        self._notify_status(overall, f"Tests completed: {passed}/{len(results)} passed")

        return TestSuiteResult(
            overall_status=overall,
            total_tests=len(results),
            passed=passed,
            failed=failed,
            duration=total_duration,
            results=results,
            logs=logs
        )

    def run_pytest_suite(self, test_paths: Optional[List[str]] = None) -> TestSuiteResult:
        """Run pytest test suite"""
        import subprocess

        config = self.config
        start_time = time.time()

        if test_paths is None:
            test_paths = config.test_patterns

        # Build pytest command
        cmd = ["python", "-m", "pytest", "-v", "--tb=short"]
        cmd.extend(test_paths)

        print(f"\n[Pytest] Running: {' '.join(cmd)}\n")
        self._notify_status(TestStatus.RUNNING, f"Running pytest: {test_paths}")

        try:
            result = subprocess.run(
                cmd,
                cwd=str(config.plugin_root),
                capture_output=True,
                text=True,
                timeout=config.test_timeout
            )

            duration = time.time() - start_time

            # Parse results
            output = result.stdout + result.stderr
            passed = output.count("PASSED")
            failed = output.count("FAILED")
            error = output.count("ERROR")

            status = TestStatus.PASSED if result.returncode == 0 else TestStatus.FAILED

            return TestSuiteResult(
                overall_status=status,
                total_tests=passed + failed + error,
                passed=passed,
                failed=failed + error,
                duration=duration,
                logs=output.split('\n')[-50:]  # Last 50 lines
            )

        except subprocess.TimeoutExpired:
            return TestSuiteResult(
                overall_status=TestStatus.TIMEOUT,
                total_tests=0,
                passed=0,
                failed=0,
                duration=config.test_timeout,
                logs=["Test suite timed out"]
            )
        except Exception as e:
            return TestSuiteResult(
                overall_status=TestStatus.FAILED,
                total_tests=0,
                passed=0,
                failed=0,
                duration=time.time() - start_time,
                logs=[str(e)]
            )

    def stop_game(self):
        """Stop the game process"""
        if self._log_monitor:
            self._log_monitor.stop()

        if self._game_proc:
            print("\n[Shutdown] Stopping game...")
            self._game_proc.terminate()
            try:
                self._game_proc.wait(timeout=10)
                print("[Shutdown] Game stopped gracefully")
            except subprocess.TimeoutExpired:
                print("[Shutdown] Force killing game...")
                self._game_proc.kill()
                self._game_proc.wait()
                print("[Shutdown] Game killed")

    def get_logs(self, level: Optional[str] = None, count: int = 100) -> List[str]:
        """Get recent logs from monitor"""
        if self._log_monitor:
            entries = self._log_monitor.get_recent(count=count, level=level)
            return [str(e) for e in entries]
        return []

    def save_session_logs(self, path: Optional[Path] = None):
        """Save all session logs to file"""
        if self._log_monitor and path:
            self._log_monitor.save_filtered_log(path)
