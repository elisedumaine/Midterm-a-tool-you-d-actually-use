"""A tiny local bridge so Tools/houdini.py can run Python inside the open Houdini.

It listens on 127.0.0.1 only, and every request must carry the secret token
written next to this file at startup.  Code runs on Houdini's main thread,
like code typed in the Python Shell, and its printed output is sent back.
"""
import contextlib
import io
import json
import os
import secrets
import socket
import threading
import traceback

PORT = 18820
TOKEN_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "iwb_bridge_token.txt")
_server = None


def _run(code):
    import hou
    output = io.StringIO()
    scope = {"hou": hou, "__name__": "__iwb_bridge__"}
    ok = True
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        try:
            exec(compile(code, "<bridge>", "exec"), scope)
        except Exception:
            traceback.print_exc()
            ok = False
    return {"ok": ok, "output": output.getvalue()}


def _handle(connection, token):
    import hdefereval
    with connection:
        data = b""
        while not data.endswith(b"\n"):
            chunk = connection.recv(65536)
            if not chunk:
                return
            data += chunk
        try:
            request = json.loads(data.decode("utf-8"))
        except ValueError:
            return
        if request.get("token") != token:
            reply = {"ok": False, "output": "wrong token"}
        elif request.get("ping"):
            import hou
            reply = {"ok": True, "output": "Houdini " + hou.applicationVersionString()}
        else:
            reply = hdefereval.executeInMainThreadWithResult(_run, request.get("code", ""))
        connection.sendall((json.dumps(reply) + "\n").encode("utf-8"))


def _serve(listener, token):
    while True:
        connection, _address = listener.accept()
        threading.Thread(target=_handle, args=(connection, token), daemon=True).start()


def start():
    global _server
    if _server:
        return
    token = secrets.token_hex(16)
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        listener.bind(("127.0.0.1", PORT))
    except OSError:
        print("IWB bridge: port {} is busy (another Houdini already runs the bridge).".format(PORT))
        return
    listener.listen(4)
    with open(TOKEN_PATH, "w") as handle:
        handle.write(token)
    _server = threading.Thread(target=_serve, args=(listener, token), daemon=True)
    _server.start()
    print("IWB bridge: Claude can talk to this Houdini on 127.0.0.1:{}".format(PORT))
