#!/usr/bin/env python3
from flask import Flask

app = Flask(__name__)

if __name__ == '__main__':

    import argparse
    import os
    import xpum_logger as logger

    logger.info('XPU Manager Prometheus Exporter Started')

    parser = argparse.ArgumentParser(
        description='Intel XPU Manager Prometheus Exporter')

    parser.add_argument('--host', default='0.0.0.0',
                        help='address on which the server listens')
    parser.add_argument('--port', default=30000,
                        help='port on which the server listens')
    parser.add_argument('--pod', action='store_true', default=None,
                        help='enable pod information retrieval from kubernetes')
    parser.add_argument('--socket_file', default=None,
                        help='socket file used by xpum daemon')

    args = parser.parse_args()

    if args.pod is not None:
        os.environ['XPUM_EXPORTER_POD'] = '1' if args.pod else '0'

    if args.socket_file is not None:
        os.environ['XPUM_SOCKET_FILE'] = args.socket_file

    from views import exporter

    # prometheus exporter
    app.add_url_rule('/metrics',
                     view_func=exporter.export_metrics, methods=['GET'])

    app.add_url_rule('/healtz',
                     view_func=exporter.check_health, methods=['GET'])

    app.run(host=args.host, port=args.port, use_reloader=False)

    logger.info('XPU Manager Prometheus Exporter Exited')
else:
    from views import exporter

    # prometheus exporter
    app.add_url_rule('/metrics',
                     view_func=exporter.export_metrics, methods=['GET'])

    app.add_url_rule('/healtz',
                     view_func=exporter.check_health, methods=['GET'])
