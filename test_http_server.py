#!/usr/bin/env python3
"""
Simple HTTP server for testing Network Boot
Serves model files from current directory

Usage:
    python test_http_server.py
    
Then in QEMU Network Boot, use:
    http://10.0.2.2:8080/stories15M.bin
"""

import http.server
import socketserver
import os

PORT = 8080

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        print(f"[HTTP] {self.address_string()} - {format%args}")

os.chdir(os.path.dirname(os.path.abspath(__file__)))

print("=" * 60)
print("  NETWORK BOOT TEST SERVER")
print("=" * 60)
print(f"  Serving files from: {os.getcwd()}")
print(f"  Server address: http://localhost:{PORT}")
print(f"  QEMU address: http://10.0.2.2:{PORT}")
print()
print("  Available models:")

for f in ["stories15M.bin", "stories110M.bin", "tokenizer.bin"]:
    if os.path.exists(f):
        size_mb = os.path.getsize(f) / (1024 * 1024)
        print(f"    - {f} ({size_mb:.1f} MB)")
        print(f"      URL: http://10.0.2.2:{PORT}/{f}")

print()
print("  Press Ctrl+C to stop server")
print("=" * 60)
print()

with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\n[Server stopped]")
