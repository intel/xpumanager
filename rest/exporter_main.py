#!/usr/bin/python3
import argparse
import os
from flask import Flask
from views import exporter

app = Flask(__name__)

# prometheus exporter
app.add_url_rule('/metrics',
                 view_func=exporter.export_metrics, methods=['GET'])

if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        description='Intel XPU Manager Prometheus Exporter')

    parser.add_argument('--host', default='0.0.0.0',
                        help='address on which the server listens')
    parser.add_argument('--port', default=30000,
                        help='port on which the server listens')
    parser.add_argument('--pod', action='store_true', default=None,
                        help='enable pod information retrieval from kubernetes')

    args = parser.parse_args()

    if args.pod is not None:
        os.environ['XPUM_EXPORTER_POD'] = '1' if args.pod else '0'

    app.run(host=args.host, port=args.port, use_reloader=False)
