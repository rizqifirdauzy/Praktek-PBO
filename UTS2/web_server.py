import os
import socket
import mimetypes
from template_engine import parse_template, compile_template

# Konfigurasi Web server
PORT = 8080
BUFFER_SIZE = 1024
FOLDER_DOCUMENT = "dokumen/"

def parse_request_line(request):
    lines = request.split("\r\n")
    request_line = lines[0]
    words = request_line.split(" ")
    if len(words) < 3:
        raise ValueError("Request line salah: kurang dari 3 elemen")
    method = words[0]
    uri = words[1]
    http_version = words[2]
    if uri.startswith('/'):
        uri = uri[1:]
    if not uri:
        uri = "index.html"
    return method, uri, http_version

def parse_get_params(uri):
    if '?' in uri:
        path, query_string = uri.split('?', 1)
        params = {}
        if query_string:
            pairs = query_string.split('&')
            for pair in pairs:
                if '=' in pair:
                    key, value = pair.split('=', 1)
                    params[key] = value
                else:
                    params[pair] = ''
        return path, params
    return uri, {}

def parse_post_data(body):
    params = {}
    if body:
        pairs = body.split('&')
        for pair in pairs:
            if '=' in pair:
                key, value = pair.split('=', 1)
                params[key] = value
            else:
                params[pair] = ''
    return params

def handle_method(method, uri, http_version, body=None):
    uri, get_params = parse_get_params(uri)
    post_params = parse_post_data(body) if body else {}
    params = {**get_params, **post_params}
    file_url = os.path.join(FOLDER_DOCUMENT, uri)
    
    if not os.path.exists(file_url):
        not_found_response = (
            f"{http_version} 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            f"Content-Length: {len('<h1>404 Not Found</h1>')}\r\n"
            "\r\n"
            "<h1>404 Not Found</h1>"
        )
        return not_found_response.encode(), len(not_found_response)
    else:
        mime_type, _ = mimetypes.guess_type(uri)
        if mime_type is None:
            mime_type = "text/html"
        with open(file_url, "r") as file:
            file_content = file.read()
            if mime_type == "text/html" and "{%" in file_content:
                tokens = parse_template(file_content)
                compiled_content = compile_template(tokens, context=params)
                response_line = (
                    f"{http_version} 200 OK\r\n"
                    "Content-Type: text/html\r\n\r\n"
                )
                response = response_line.encode() + compiled_content.encode('utf-8')
                return response, len(response)
            else:
                with open(file_url, "rb") as file:
                    file_content = file.read()
                response_line = (
                    f"{http_version} 200 OK\r\n"
                    f"Content-Type: {mime_type}\r\n\r\n"
                )
                response = response_line.encode() + file_content
                return response, len(response)

def handle_client(sock_client):
    request = sock_client.recv(BUFFER_SIZE).decode('utf-8', errors='ignore')
    print("-----------------------------------------------")
    print("Request dari browser:\r\n\r\n", request)
    method, uri, http_version = parse_request_line(request)
    
    # Handling POST data if present
    body = None
    if method == 'POST':
        headers_body_split = request.split("\r\n\r\n")
        if len(headers_body_split) > 1:
            body = headers_body_split[1]
    
    response, response_size = handle_method(method, uri, http_version, body)
    if response:
        sock_client.sendall(response)
    else:
        print("Response data is NULL")
    
    print("-----------------------------------------------")
    print(f"Method: {method} URI: {uri}")
    print("Response ke browser:\n", response.decode('utf-8', errors='ignore'))
    sock_client.close()

def main():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('', PORT))
    server_socket.listen(3)
    print(f"Server siap di IP: 127.0.0.1 Port: {PORT}")
    while True:
        client_socket, addr = server_socket.accept()
        handle_client(client_socket)
    server_socket.close()

if __name__ == "__main__":
    main()