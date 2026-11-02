"""Run Python inside the open Unreal Editor, through Unreal's Python remote execution.

The editor must be running this project (remote execution is enabled in
Config/DefaultEngine.ini, local machine only).

    python Tools/ue.py "print(unreal.SystemLibrary.get_engine_version())"
    python Tools/ue.py -f Content/Python/reload_world_builder.py
    python Tools/ue.py --ping

Prints whatever the code prints or logs; exits 1 if the code raised.
"""
import argparse
import os
import sys
import time

ENGINE_PYTHON = r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python"
sys.path.insert(0, ENGINE_PYTHON)
import remote_execution as rx  # noqa: E402


def find_editor(remote, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        nodes = remote.remote_nodes
        if nodes:
            return nodes[0]
        time.sleep(0.2)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("code", nargs="?", help="Python statements to run in the editor")
    parser.add_argument("-f", "--file", help="Python file to run in the editor")
    parser.add_argument("--ping", action="store_true", help="only check that the editor answers")
    parser.add_argument("--timeout", type=float, default=5, help="seconds to wait for the editor (default 5)")
    args = parser.parse_args()
    if not (args.code or args.file or args.ping):
        parser.error("give code, -f FILE or --ping")

    remote = rx.RemoteExecution()
    remote.start()
    try:
        node = find_editor(remote, args.timeout)
        if not node:
            print("Unreal Editor not found. Is the project open in Unreal (and was it restarted after enabling remote execution)?")
            return 2
        if args.ping:
            print("Unreal Editor is answering:", node.get("project_name", ""), node.get("engine_version", ""))
            return 0
        remote.open_command_connection(node["node_id"])
        if args.file:
            result = remote.run_command(os.path.abspath(args.file), unattended=True, exec_mode=rx.MODE_EXEC_FILE)
        else:
            result = remote.run_command("import unreal\n" + args.code, unattended=True, exec_mode=rx.MODE_EXEC_FILE)
        for line in result.get("output", []):
            print(line.get("output", "").rstrip())
        if result.get("result") not in (None, "None", ""):
            print(result["result"])
        return 0 if result.get("success") else 1
    finally:
        remote.stop()


if __name__ == "__main__":
    sys.exit(main())
