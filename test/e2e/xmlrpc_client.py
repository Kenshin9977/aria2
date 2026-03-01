#!/usr/bin/env python3
"""Minimal XML-RPC client for aria2 e2e tests.

Usage:
  python3 xmlrpc_client.py --port PORT --method METHOD [--params JSON]
"""

import argparse
import json
import sys
import xmlrpc.client


def main():
    parser = argparse.ArgumentParser(
        description="XML-RPC client for aria2 e2e tests")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--method", required=True)
    parser.add_argument("--params", default=None,
                        help="JSON array of parameters")
    args = parser.parse_args()

    proxy = xmlrpc.client.ServerProxy(
        f"http://127.0.0.1:{args.port}/rpc")
    method_func = getattr(proxy, args.method)

    try:
        if args.params:
            params = json.loads(args.params)
            if not isinstance(params, list):
                params = [params]
            result = method_func(*params)
        else:
            result = method_func()
        print(json.dumps(result, default=str))
    except xmlrpc.client.Fault as e:
        print(json.dumps({"fault": str(e)}), file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": str(e)}), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
