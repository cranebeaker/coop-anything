import json
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'runtime-libs'))
try:
    import frida
except ModuleNotFoundError:
    # In-repo layout: <repo>/games/<game>/tools -> <repo>/local/runtime-libs holds the pinned wheel.
    fallback = ROOT.parents[2] / 'local' / 'runtime-libs'
    if fallback.is_dir():
        sys.path.insert(0, str(fallback))
    import frida


def run_guarded(argv, config, duration=15, cwd=None, environment=None, observer=None, observation_delay=0, during=None, extra_scripts=None):
    device = frida.get_local_device()
    ready = threading.Event()
    detached = threading.Event()
    events = []
    errors = []
    session = None
    pid = None
    result = {'events': events, 'errors': errors, 'resumed': False, 'output': []}

    def on_output(output_pid, fd, data):
        if output_pid == pid and len(result['output']) < 40:
            result['output'].append({'fd': fd, 'text': data[:2048].decode('utf-8', errors='replace')})

    device.on('output', on_output)

    def on_message(message, data):
        if message['type'] == 'send':
            payload = message['payload']
            events.append(payload)
            if payload.get('kind') == 'guard-ready':
                ready.set()
        elif message['type'] == 'error':
            errors.append(message.get('stack', message.get('description')))
            ready.set()

    try:
        pid = device.spawn(argv, cwd=str(cwd or ROOT), stdio='pipe',
                           env={'PYTHONDONTWRITEBYTECODE': '1', 'TEMP': str(ROOT / 'tmp'), 'TMP': str(ROOT / 'tmp'), **(environment or {})})
        result['pid'] = pid
        session = device.attach(pid)
        session.on('detached', lambda reason, crash=None: (result.update(detach_reason=reason, crash=str(crash) if crash else None), detached.set()))
        source = (ROOT / 'save_guard.js').read_text(encoding='utf-8').replace('__CONFIG__', json.dumps(config))
        script = session.create_script(source, name='south-park-save-guard')
        script.on('message', on_message)
        script.load()
        if not ready.wait(5) or errors:
            raise RuntimeError('Guard failed before resume: ' + repr(errors))
        for extra_name in (extra_scripts or []):
            extra = session.create_script((ROOT / extra_name).read_text(encoding='utf-8'), name=extra_name)
            extra.on('message', on_message)
            extra.load()
            result.setdefault('extra_scripts', {})[extra_name] = extra
        device.resume(pid)
        result['resumed'] = True
        if during is not None:
            result['during'] = during(pid, script)
        result['exited_before_deadline'] = detached.wait(duration)
        if not result['exited_before_deadline']:
            if observation_delay:
                time.sleep(observation_delay)
            result['telemetry'] = script.exports_sync.telemetry()
            for extra_name, extra in (result.get('extra_scripts') or {}).items():
                try:
                    result.setdefault('extra_telemetry', {})[extra_name] = extra.exports_sync.telemetry()
                except Exception as extra_error:
                    result.setdefault('extra_errors', {})[extra_name] = repr(extra_error)
            if observer is not None:
                result['observation'] = observer(pid, script)
    finally:
        device.off('output', on_output)
        if pid is not None:
            try:
                device.kill(pid)
            except frida.ProcessNotFoundError:
                pass
        if session is not None:
            try:
                session.detach()
            except frida.InvalidOperationError:
                pass
    return result
