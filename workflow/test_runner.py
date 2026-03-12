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

def get_desktop_path():
    if os.name == 'nt':
        desktop_path = os.path.join(os.environ['USERPROFILE'], 'Desktop')
    # macOS/Linux 系统
    else:
        desktop_path = os.path.join(os.path.expanduser('~'), 'Desktop')
    return desktop_path




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
            print(f"ERROR|Launch|Executable not found: {exe_path}")
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
            "-RenderOffScreen",
        ]

        if extra_args:
            cmd.extend(extra_args)

        print(f"INFO|Launch|Starting {exe_path.name}")

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
            self._log_monitor.filter.include_levels = config.log_include_levels
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
        print(f"INFO|Server|Waiting for server on port {config.port}")

        while time.time() - start_time < timeout:
            if self._cancelled:
                return False

            # Check if process died
            if self._game_proc and self._game_proc.poll() is not None:
                exit_code = self._game_proc.returncode
                print(f"ERROR|Server|Game exited with code {exit_code}")
                return False

            # Check TCP port
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                result = sock.connect_ex((config.host, config.port))
                sock.close()
                if result == 0:
                    elapsed = time.time() - start_time
                    print(f"INFO|Server|Ready after {elapsed:.1f}s")
                    self._server_ready = True
                    return True
            except Exception:
                pass

            time.sleep(0.5)

        print(f"ERROR|Server|Timeout after {timeout}s")
        return False

    def run_tests(self) -> TestSuiteResult:
        import unrealcv

        config = self.config
        start_time = time.time()
        results = []

        self._notify_status(TestStatus.RUNNING, "Running tests")

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

        # Wait for game to fully initialize (shader compilation, etc.)
        if config.post_launch_delay > 0:
            self._notify_status(TestStatus.RUNNING, f"Waiting {config.post_launch_delay}s for game initialization...")
            print(f"INFO|Test|Waiting {config.post_launch_delay}s for initialization")
            time.sleep(config.post_launch_delay)

        # Define tests
        tests = [
            # === 插件/基础命令 ===
            ("Unrealcv Version", "vget /unrealcv/version"),
            ("Unrealcv Status", "vget /unrealcv/status"),
            ("Unrealcv Help", "vget /unrealcv/help"),
            ("Unrealcv Echo", "vget /unrealcv/echo test_message"),
            ("Scene Name", "vget /scene/name"),
            ("Level Name", "vget /level/name"),

            # === 摄像机命令 ===
            ("Cameras List", "vget /cameras"),
            ("Cameras CID Format", "vget /cameras_CID"),
            ("Cameras Legacy Format", "vget /cameras_legacy"),
            ("Camera 0 Location", "vget /camera/0/location"),
            ("Camera 0 Rotation", "vget /camera/0/rotation"),
            ("Camera 0 FOV", "vget /camera/0/fov"),
            ("Camera 0 Size", "vget /camera/0/size"),
            ("Camera 0 Use Fast Capture", "vget /camera/0/use_fast_capture"),

            # === 物体命令 ===
            ("Objects List", "vget /objects"),
            ("Objects Search bp", "vget /objects bp"),
            # ("Objects Scan Assets", "vget /objects/scan_assets"),  # OOM

            # === 游戏控制 ===
            ("Is Paused", "vget /action/game/is_paused"),

            # === Pawn 命令 ===
            ("Pawn Location", "vget /pawn/location"),
            ("Pawn Rotation", "vget /pawn/rotation"),

            # === 光照命令 ===
            ("Directional Light Intensity", "vget /light/directional/intensity"),
            ("Skylight Intensity", "vget /light/skylight/intensity"),
            ("Directional Light Cast Deep Shadow", "vget /light/directional/castdeepshadow"),

            # === 视图模式 ===
            ("View Mode", "vget /viewmode"),

            # === 数据集自动化 ===
            ("DatasetAutomation Task Name", "vget /datasetautomation/task_name"),
            ("DatasetAutomation Status", "vget /datasetautomation/status"),
            ("DatasetAutomation History", "vget /datasetautomation/history"),
            ("DatasetAutomation Sequence", "vget /datasetautomation/sequence"),
            ("DatasetAutomation Config B Exit On Complete", "vget /datasetautomation/config/b_exit_on_complete"),

            # === PAK 文件命令 ===
            ("PAK Mounted List", "vget /pak/mounted"),

            # === 记录/CaptureActor - 全局设置 ===
            ("CaptureActor Asset Pool", "vget /captureactor/asset_pool"),
            ("CaptureActor Time Dilation", "vget /captureactor/time_dilation"),
            ("CaptureActor Use Movie Quality Rendering", "vget /captureactor/use_movie_quality_rendering"),
            ("CaptureActor Record Via Viewport", "vget /captureactor/record_via_viewport"),
            ("CaptureActor Warmup Frames", "vget /captureactor/warmup_frames"),
            ("CaptureActor Paused Tick Interval", "vget /captureactor/paused_tick_interval"),

            # === 记录/CaptureActor - 视频编码设置 ===
            ("CaptureActor Video Encoder Bitrate", "vget /captureactor/video_encoder_bitrate"),
            ("CaptureActor H264 Encoding", "vget /captureactor/h264_encoding"),
            ("CaptureActor Auto Generate Video", "vget /captureactor/auto_generate_video"),
            ("CaptureActor Video Gen Script Path", "vget /captureactor/video_gen_script_path"),

            # # === 记录/CaptureActor - 每摄像机设置 (camera 0) ===
            # ("CaptureActor 0 Add Timestamp", "vget /captureactor/0/add_timestamp"),
            # ("CaptureActor 0 Paused", "vget /captureactor/0/paused"),

            # # === 记录/CaptureActor - 前景运动控制 (camera 0) ===
            # ("CaptureActor 0 Track Foreground Movement", "vget /captureactor/0/track_foreground_movement"),
            # ("CaptureActor 0 Foreground Move Speed", "vget /captureactor/0/foreground_move_speed"),
            # ("CaptureActor 0 Foreground Move Angle Offset", "vget /captureactor/0/foreground_move_angle_offset"),
            # ("CaptureActor 0 Target Height Offset", "vget /captureactor/0/target_height_offset"),

            # # === 记录/CaptureActor - 轨迹模式 (camera 0) ===
            # ("CaptureActor 0 Bullet Time Speed", "vget /captureactor/0/bullet_time_speed"),

            # === 记录/CaptureActor - 随机轨迹设置 ===
            ("CaptureActor Random Distance Variation Probability", "vget /captureactor/random_distance_variation_probability"),
            ("CaptureActor Distance Variation Min", "vget /captureactor/distance_variation_min"),
            ("CaptureActor Distance Variation Max", "vget /captureactor/distance_variation_max"),

            # === 别名/关卡命令 ===
            ("Persistent Level ID", "vget /persistent_level/id"),
            # ("Persistent Level Script Actor ID", "vget /persistent_level/level_script_actor/id"),

            # === MetaHuman 命令 ===
            ("MetaHuman All Paths", "vget /metahuman/all_paths"),
            ("MetaHuman Cache Path", "vget /metahuman/cache_path"),
            ("MetaHuman Filter Batch", "vget /metahuman/filter_batch"),

            # === MVRC 命令 ===
            ("MVRC Use Sync Capture", "vget /mvrc/use_sync_capture"),

            # === MQRC 命令 - Anti-Aliasing & Screen Percentage ===
            ("MQRC Antialiasing", "vget /mqrc/antialiasing"),
            ("MQRC Screen Percentage", "vget /mqrc/screen_percentage"),
            ("MQRC Screen Percentage Method", "vget /mqrc/screen_percentage_method"),

            # === MQRC 命令 - Exposure Control ===
            ("MQRC Exposure Method", "vget /mqrc/exposure_method"),
            ("MQRC Exposure Bias", "vget /mqrc/exposure_bias"),
            ("MQRC Auto Exposure Min Brightness", "vget /mqrc/auto_exposure_min_brightness"),
            ("MQRC Auto Exposure Max Brightness", "vget /mqrc/auto_exposure_max_brightness"),

            # === MQRC 命令 - Color Correction ===
            ("MQRC Saturation", "vget /mqrc/saturation"),
            ("MQRC Contrast", "vget /mqrc/contrast"),
            ("MQRC Gamma", "vget /mqrc/gamma"),
            ("MQRC Gain", "vget /mqrc/gain"),

            # === MQRC 命令 - Motion & Depth Effects ===
            ("MQRC Motion Blur", "vget /mqrc/motion_blur"),
            ("MQRC Depth Of Field Scale", "vget /mqrc/depth_of_field_scale"),

            # === MQRC 命令 - Lumen Global Illumination ===
            ("MQRC Lumen Quality", "vget /mqrc/lumen_quality"),
            ("MQRC Lumen Final Gather Lighting Update Speed", "vget /mqrc/lumen_final_gather_lighting_update_speed"),
            ("MQRC Override Lumen Final Gather Lighting Update Speed", "vget /mqrc/override_lumen_final_gather_lighting_update_speed"),

            # === 编辑器命令 ===
            # ("Editor Start Standalone PIE", "vset /editor/start_standalone_pie"),  # Editor only

            # === 数据集自动化 - 额外命令 ===
            # ("DatasetAutomation Config Recording Options", "vset /datasetautomation/config/recording_options lit,mask,normal,depth"),  # Need file path validation
            # ("DatasetAutomation Start With Log Suffix", "vset /datasetautomation/start test_session"),  # May require active setup
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
                    res_to_print = res
                    res = res.strip()
                    next_line_index = res.find("\n")
                    if next_line_index != -1:
                        res_to_print = res_to_print[:next_line_index]
                    if len(res_to_print) > 100:
                        res_to_print = res_to_print[:100] + f"... ({len(res)} chars)"
                    results.append(TestResult(
                        name=name,
                        status=TestStatus.PASSED,
                        duration=duration,
                        message=f"Response: {res_to_print}"
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

        # Image capture tests
        if not self._cancelled:
            desktop_path = get_desktop_path()
            assert not " " in desktop_path, "the path should not contain space, otherwise the vget UE console parser will fail"

            # 构造测试命令列表（自动拼接桌面路径）
            capture_tests = [
                ("Capture Lit", f"vget /camera/0/lit {os.path.join(desktop_path, 'test_lit.png')}"),
                ("Capture Depth", f"vget /camera/0/depth {os.path.join(desktop_path, 'test_depth.npy')}"),
                ("Capture Normal", f"vget /camera/0/normal {os.path.join(desktop_path, 'test_normal.png')}"),
                ("Capture ObjectMask", f"vget /camera/0/object_mask {os.path.join(desktop_path, 'test_mask.png')}"),
                ("Capture OpticalFlow", f"vget /camera/0/optical_flow {os.path.join(desktop_path, 'test_flow.png')}"),
            ]

            print("INFO|Test|Running image capture tests")
            for name, cmd in capture_tests:
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

                    # Check if response is valid (not error and has content)
                    if res and not res.startswith("error"):
                        # Try to parse as image data (should be binary PNG data or file path)
                        is_valid = len(res) > 100 or res.endswith('.png') or res.endswith('.exr') or res.endswith('.npy')
                        if is_valid:
                            results.append(TestResult(
                                name=name,
                                status=TestStatus.PASSED,
                                duration=duration,
                                message=f"Captured: {len(res)} bytes"
                            ))
                        else:
                            results.append(TestResult(
                                name=name,
                                status=TestStatus.FAILED,
                                duration=duration,
                                message=f"Invalid response: {res[:100]}"
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

        # Performance tests - 50 iterations for each sensor type and mode
        if not self._cancelled:
            # print("\n" + "="*60)
            # print("INFO|Test|Starting Performance Tests (50 iterations each)")
            # print("="*60)

            # print("INFO|Test|Waiting for 10 seconds to let the UE Game settle...")
            time.sleep(10)

            # Define sensor types with their file extensions
            sensor_configs = [
                ("lit", "png"),
                ("depth", "npy"),
                ("normal", "png"),
                ("object_mask", "png"),
                ("optical_flow", "png"),
            ]

            performance_iterations = 50

            for sensor_type, file_ext in sensor_configs:
                if self._cancelled:
                    break

                # print(f"\n--- Performance Test: {sensor_type} ---")

                # === Test 1: File path mode (direct save to disk) ===
                file_times = []
                file_cmd = f"vget /camera/0/{sensor_type} {os.path.join(desktop_path, f'perf_{sensor_type}.{file_ext}')}"

                # print(f"  [File Mode] Running {performance_iterations} iterations...")
                for i in range(performance_iterations):
                    if self._cancelled:
                        break
                    iter_start = time.time()
                    try:
                        res = client.request(file_cmd)
                        iter_duration = time.time() - iter_start
                        if res and not res.startswith("error"):
                            file_times.append(iter_duration)
                    except Exception:
                        pass  # Skip failed iterations

                file_avg = sum(file_times) / len(file_times) if file_times else 0
                file_fps = 1.0 / file_avg if file_avg > 0 else 0
                file_total = sum(file_times)

                # === Test 2: Suffix mode (TCP transfer) ===
                suffix_times = []
                suffix_cmd = f"vget /camera/0/{sensor_type} {file_ext}"

                # print(f"  [TCP Mode]  Running {performance_iterations} iterations...")
                for i in range(performance_iterations):
                    if self._cancelled:
                        break
                    iter_start = time.time()
                    try:
                        res = client.request(suffix_cmd)
                        iter_duration = time.time() - iter_start
                        if res and (not isinstance(res, str) or not res.startswith("error")):
                            suffix_times.append(iter_duration)
                    except Exception as e:
                        print(f"Error in suffix mode iteration {i}: {e}")
                        pass  # Skip failed iterations

                suffix_avg = sum(suffix_times) / len(suffix_times) if suffix_times else 0
                suffix_fps = 1.0 / suffix_avg if suffix_avg > 0 else 0
                suffix_total = sum(suffix_times)

                # # === Report Results ===
                # print(f"\n  [{sensor_type.upper()}] Performance Summary:")
                # print(f"    File Mode (direct save):  {len(file_times)}/{performance_iterations} success")
                # print(f"      Total: {file_total:.2f}s | Avg: {file_avg*1000:.1f}ms | FPS: {file_fps:.1f}")
                # print(f"    TCP Mode (network transfer): {len(suffix_times)}/{performance_iterations} success")
                # print(f"      Total: {suffix_total:.2f}s | Avg: {suffix_avg*1000:.1f}ms | FPS: {suffix_fps:.1f}")

                # Add performance test results to test suite
                results.append(TestResult(
                    name=f"Perf-{sensor_type}-File-{performance_iterations}x",
                    status=TestStatus.PASSED if len(file_times) > 0 else TestStatus.FAILED,
                    duration=file_total,
                    message=f"FPS: {file_fps:.1f} ({len(file_times)}/{performance_iterations} success)"
                ))

                results.append(TestResult(
                    name=f"Perf-{sensor_type}-TCP-{performance_iterations}x",
                    status=TestStatus.PASSED if len(suffix_times) > 0 else TestStatus.FAILED,
                    duration=suffix_total,
                    message=f"FPS: {suffix_fps:.1f} ({len(suffix_times)}/{performance_iterations} success)"
                ))



        if not self._cancelled:
            _cnt = 0
            _output_dir = os.path.abspath(os.path.join(desktop_path, 'DatasetAutomationOutputDirectory'))
            _set_output_result = client.request(f"vset /datasetautomation/config/output_directory {_output_dir}")
            _set_total_scenes_result = client.request(f"vset /datasetautomation/config/total_scenes 2")
            _set_exit_on_complete_result = client.request("vset /datasetautomation/config/b_exit_on_complete false")
            _start_result = client.request("vset /datasetautomation/start")
            print(end="\n")
            _check = lambda x: isinstance(x, str) and not x.lower().startswith("error")
            if _check(_start_result) and _check(_set_output_result) and _check(_set_total_scenes_result) and _check(_set_exit_on_complete_result):
                while True:
                    _cnt += 1
                    try:
                        status = client.request("vget /datasetautomation/status")
                        
                        anim = ['⠁','⠈','⠐','⠠','⢀','⡀','⠄','⠂',]

                        print(f"\r{anim[_cnt % len(anim)]} {status}", end="")

                        if status is None:
                            results.append(TestResult(
                                name="Dataset Automation Status",
                                status=TestStatus.FAILED,
                                duration=0,
                                message=f"Status is None: {status}"
                            ))
                            break

                        if "Completed" in status:
                            results.append(TestResult(
                                name="Dataset Automation Status",
                                status=TestStatus.PASSED,
                                duration=0,
                                message=f"Session completed: {status}, output directory: {_output_dir}"
                            ))
                            break
                        elif "Error" in status:
                            results.append(TestResult(
                                name="Dataset Automation Status",
                                status=TestStatus.FAILED,
                                duration=0,
                                message=f"Session failed: {status}"
                            ))
                            break

                    except Exception as e:
                        print(f"[ERROR] Status check failed: {e}")
                        break

                    time.sleep(1.0)
            else:
                results.append(TestResult(
                    name="Dataset Automation Start",
                    status=TestStatus.FAILED,
                    duration=0,
                    message=f"Failed to set output directory or total scenes or exit on complete or start dataset automation: {_set_output_result}, {_set_total_scenes_result}, {_set_exit_on_complete_result}, {_start_result}"
                ))
            
            print(end='\r')



        # Calculate summary
        passed = sum(1 for r in results if r.status == TestStatus.PASSED)
        failed = sum(1 for r in results if r.status == TestStatus.FAILED)
        total_duration = time.time() - start_time

        # Get recent errors from log monitor
        logs = []
        critical_error_logs = []
        if self._log_monitor:
            errors = self._log_monitor.get_errors()

            # Check for critical errors: must match BOTH category AND Error level
            critical_categories = {"LogUnrealCV", "LogTemp"}
            for error in errors:
                # Error is already Error level, check if category matches
                if any(cat.lower() in error.category.lower() for cat in critical_categories):
                    critical_error_logs.append(error)

        # Fail test only if critical category + Error level found
        if critical_error_logs and failed == 0:
            failed += 1
            # Add synthetic test result for log errors
            error_msg = f"Critical errors found in logs: {', '.join(str(e) for e in critical_error_logs)}"
            results.append(TestResult(
                name="Log Error Check",
                status=TestStatus.FAILED,
                duration=0,
                message=error_msg
            ))

        overall = TestStatus.PASSED if failed == 0 else TestStatus.FAILED

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



    def stop_game(self):
        """Stop the game process"""
        if self._log_monitor:
            self._log_monitor.stop()

        if self._game_proc:
            print("INFO|Shutdown|Stopping game")
            self._game_proc.terminate()
            try:
                self._game_proc.wait(timeout=10)
                print("INFO|Shutdown|Game stopped gracefully")
            except subprocess.TimeoutExpired:
                print("INFO|Shutdown|Force killing game")
                self._game_proc.kill()
                self._game_proc.wait()
                print("INFO|Shutdown|Game killed")

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
