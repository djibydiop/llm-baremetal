# HTTP Model Server with Range Support
# Serves model files with byte-range support for UEFI streaming

import http.server
import socketserver
import os
import re
import sys

PORT = 8080

class RangeHandler(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        path = self.translate_path(self.path)
        
        if os.path.isdir(path):
            return super().send_head()
        
        try:
            f = open(path, 'rb')
        except OSError:
            self.send_error(404)
            return None
        
        try:
            file_size = os.path.getsize(path)
            
            # Check Range header
            range_header = self.headers.get('Range')
            if range_header:
                match = re.match(r'bytes=(\d+)-(\d*)', range_header)
                if match:
                    start = int(match.group(1))
                    end = int(match.group(2)) if match.group(2) else file_size - 1
                    
                    if start >= file_size or end >= file_size or start > end:
                        self.send_error(416)
                        return None
                    
                    # 206 Partial Content
                    self.send_response(206)
                    self.send_header("Content-Type", "application/octet-stream")
                    self.send_header("Content-Range", f"bytes {start}-{end}/{file_size}")
                    self.send_header("Content-Length", str(end - start + 1))
                    self.send_header("Accept-Ranges", "bytes")
                    self.end_headers()
                    
                    f.seek(start)
                    return f
            
            # 200 OK (full file)
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(file_size))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            return f
            
        except:
            f.close()
            self.send_error(500)
            return None

print(f"Starting HTTP server on port {PORT}")
print(f"Access from QEMU: http://10.0.2.2:{PORT}/")
print(f"Serving: {os.getcwd()}")
print("\nPress Ctrl+C to stop\n")

with socketserver.TCPServer(("", PORT), RangeHandler) as httpd:
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped")
