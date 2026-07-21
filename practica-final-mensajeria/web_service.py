#!/usr/bin/env python3
"""
web_service.py - Conversor de mensajes (servicio web)
Sistemas Distribuidos - UC3M - Curso 2025-2026

Elimina espacios en blanco repetidos de los mensajes.
Se despliega en la máquina local del cliente (localhost:8081).

Uso:
    python3 web_service.py

Endpoint:
    POST /normalize
    Body JSON: {"message": "texto   con   espacios"}
    Response JSON: {"message": "texto con espacios"}
"""

from flask import Flask, request, jsonify
import re

app = Flask(__name__)


@app.route('/normalize', methods=['POST'])
def normalize():
    """Elimina espacios en blanco repetidos del mensaje."""
    data = request.get_json(silent=True)
    if not data or 'message' not in data:
        return jsonify({'error': 'Missing message field'}), 400
    
    # Reemplazar múltiples espacios/tabs por un único espacio
    normalized = re.sub(r'[ \t]+', ' ', data['message']).strip()
    return jsonify({'message': normalized})


@app.route('/health', methods=['GET'])
def health():
    """Comprobación de estado del servicio."""
    return jsonify({'status': 'ok'}), 200


if __name__ == '__main__':
    print("s> Conversor de mensajes iniciado en http://localhost:8081")
    app.run(host='0.0.0.0', port=8081, debug=False)
