"""Run Python inside the open Houdini, through the local IWB bridge.

Houdini starts the bridge itself (Documents/houdini22.0/python3.13libs/uiready.py).
`hou` is already imported.

    python Tools/houdini.py --ping
    python Tools/houdini.py "print(hou.node('/obj').children())"
    python Tools/houdini.py -f path/to/script.py

Prints whatever the code prints; exits 1 if the code raised.
"""
import argparse
import json
import os
import socket
import sys

PORT = 18820
TOKEN_PATH = os.path.join(os.path.expanduser("~"), "Documents", "houdini22.0", "iwb_bridge_token.txt")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("code", nargs="?", help="Python statements to run in Houdini")
    parser.add_argument("-f", "--file", help="Python file to run in Houdini")
    parser.add_argument("--ping", action="store_true", help="only check that Houdini answers")
    parser.add_argument("--timeout", type=float, default=600, help="seconds to wait for the result (default 600)")
    args = parser.parse_args()
    if not (args.code or args.file or args.ping):
        parser.error("give code, -f FILE or --ping")
    try:
        token = open(TOKEN_PATH).read().strip()
    except OSError:
        sys.exit("Houdini bridge token not found. Is Houdini open (started after the bridge was installed)?")
    request = {"token": token}
    if args.ping:
        request["ping"] = True
    else:
        request["code"] = open(args.file, encoding="utf-8").read() if args.file else args.code
    try:
        with socket.create_connection(("127.0.0.1", PORT), timeout=5) as connection:
            connection.settimeout(args.timeout)
            connection.sendall((json.dumps(request) + "\n").encode("utf-8"))
            data = b""
            while not data.endswith(b"\n"):
                chunk = connection.recv(65536)
                if not chunk:
                    break
                data += chunk
    except OSError as error:
        sys.exit("Houdini is not answering ({}). Is it open?".format(error))
    reply = json.loads(data.decode("utf-8"))
    sys.stdout.write(reply["output"])
    sys.exit(0 if reply["ok"] else 1)


if __name__ == "__main__":
    main()
