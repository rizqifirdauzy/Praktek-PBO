import re

def parse_template(text):
    delimiter = re.compile(r'{%(.*?)%}', re.DOTALL)
    tokens = []
    for index, token in enumerate(delimiter.split(text)):
        if index % 2 == 0:
            # Kelompok text bukan program (HTML biasa).
            # Namun jika ditemukannya token program,
            # maka tambahkan script ke daftar token
            if token:
                tokens.append((False, token.replace('%\}', '%}').replace('{\%', '{%')))
        else:
            # Kelompok text program
            # Cari Indentasi
            # karena aturan penulisan program pada Python tergantung indentasi
            lines = token.replace('{\%', '{%').replace('%\}', '%}').splitlines()
            # Menentukan indentasi minimum
            indent = None
            for l in lines:
                # Hanya pertimbangkan baris yang tidak kosong
                if l.strip():
                    current_indent = len(l) - len(l.lstrip())
                    if indent is None or current_indent < indent:
                        indent = current_indent
            # Menghilangkan indentasi dari setiap baris
            realigned_lines = []
            for l in lines:
                realigned_lines.append(l[indent:])
            realigned = '\n'.join(realigned_lines)
            # Tambahkan ke daftar token,
            # sekaligus kompilasi script menjadi object code
            tokens.append((True, compile(realigned, '<template> %s' % realigned[:20], 'exec')))
    return tokens

def compile_template(tokens, context=None, **keyword_args):
    global_context = {}
    if context:
        global_context.update(context)
    if keyword_args:
        global_context.update(keyword_args)

    # Tambahkan fungsi untuk output
    # echo untuk menampilkan data tanpa format
    # echo_fmt menampilkan data dengan format tertentu
    result = []

    def echo(*args):
        result.extend([str(arg) for arg in args])

    def echo_fmt(fmt, *args):
        result.append(fmt % args)

    global_context['echo'] = echo
    global_context['echo_fmt'] = echo_fmt

    # Jika statusnya code, maka jalankan program
    # Selain status code, tampilkan saja
    for is_code, token in tokens:
        if is_code:
            exec(token, global_context)
        else:
            result.append(token)

    return ''.join(result)