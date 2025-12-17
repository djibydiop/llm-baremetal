#!/usr/bin/env python3
"""
DRC Network Consensus Validator Server
Made in Senegal 🇸🇳

Ce serveur valide les requêtes de boot des PCs bare-metal.
Logique DRC-lite: vérifie que le système peut booter en sécurité.
"""

from flask import Flask, request, jsonify
from datetime import datetime
import hashlib
import json

app = Flask(__name__)

# Configuration du validateur
VALIDATOR_ID = "validator-1"
VALIDATOR_VERSION = "1.0.0"

# Statistiques
stats = {
    "total_requests": 0,
    "approved": 0,
    "rejected": 0,
    "start_time": datetime.now().isoformat()
}

# Règles DRC basiques
DRC_RULES = {
    "max_boot_per_hour": 100,  # Anti-spam
    "require_model_hash": True,  # Vérifier intégrité
    "require_drc_version": "5.1",  # Version DRC minimum
    "block_suspicious_patterns": True  # Patterns dangereux
}

# Hash connus des modèles autorisés
APPROVED_MODEL_HASHES = {
    "stories15M": "placeholder_hash_stories15M",  # À remplacer par vrai hash
    "stories42M": "placeholder_hash_stories42M",
    "stories110M": "placeholder_hash_stories110M"
}


def validate_boot_request(data):
    """
    Valide une requête de boot selon les règles DRC
    
    Args:
        data: dict avec system_state, model_info, drc_version
        
    Returns:
        (approved: bool, reason: str)
    """
    
    # Règle 1: Vérifier version DRC
    if "drc_version" not in data:
        return False, "Missing DRC version"
    
    if data["drc_version"] != DRC_RULES["require_drc_version"]:
        return False, f"DRC version mismatch: got {data['drc_version']}, expected {DRC_RULES['require_drc_version']}"
    
    # Règle 2: Vérifier modèle (si hash fourni)
    if DRC_RULES["require_model_hash"] and "model_hash" in data:
        model_name = data.get("model_name", "")
        model_hash = data.get("model_hash", "")
        
        # Pour l'instant, on accepte tous les hashes (à améliorer)
        if len(model_hash) < 10:
            return False, "Invalid model hash"
    
    # Règle 3: Vérifier patterns suspects
    if DRC_RULES["block_suspicious_patterns"]:
        system_state = data.get("system_state", {})
        
        # Exemple: bloquer si trop de tentatives échouées
        if system_state.get("failed_boots", 0) > 5:
            return False, "Too many failed boots detected"
    
    # Règle 4: Vérifier token 3 dans historique (DRC core rule)
    if "last_tokens" in data:
        tokens = data["last_tokens"]
        if 3 in tokens and is_dangerous_context(tokens, tokens.index(3)):
            return False, "DRC: Token 3 in dangerous context detected"
    
    # Toutes les règles passées
    return True, "All DRC rules passed"


def is_dangerous_context(tokens, token3_index):
    """
    Vérifie si le token 3 est dans un contexte dangereux
    (Logique simplifiée de DRC v5.1)
    """
    if token3_index < 2 or token3_index >= len(tokens) - 2:
        return False  # Pas assez de contexte
    
    # Pattern dangereux: [X, Y, 3, Z, W] où Z est dans dangerous_set
    dangerous_after = {5, 7, 11, 13}  # Exemple de tokens dangereux
    
    if token3_index + 1 < len(tokens):
        next_token = tokens[token3_index + 1]
        if next_token in dangerous_after:
            return True
    
    return False


@app.route('/health', methods=['GET'])
def health():
    """Health check endpoint"""
    return jsonify({
        "status": "healthy",
        "validator_id": VALIDATOR_ID,
        "version": VALIDATOR_VERSION,
        "uptime": (datetime.now() - datetime.fromisoformat(stats["start_time"])).total_seconds()
    })


@app.route('/validate', methods=['POST'])
def validate():
    """
    Endpoint principal de validation
    
    Request body:
    {
        "system_state": {
            "failed_boots": 0,
            "last_boot_time": "2025-12-16T10:00:00Z"
        },
        "model_name": "stories15M",
        "model_hash": "abc123...",
        "drc_version": "5.1",
        "last_tokens": [1, 2, 3, 4, 5]
    }
    
    Response:
    {
        "approved": true/false,
        "reason": "...",
        "validator_id": "validator-1",
        "timestamp": "2025-12-16T10:00:01Z"
    }
    """
    
    stats["total_requests"] += 1
    
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                "approved": False,
                "reason": "Invalid JSON",
                "validator_id": VALIDATOR_ID,
                "timestamp": datetime.now().isoformat()
            }), 400
        
        # Valider la requête
        approved, reason = validate_boot_request(data)
        
        # Mettre à jour stats
        if approved:
            stats["approved"] += 1
        else:
            stats["rejected"] += 1
        
        # Log
        print(f"[{datetime.now().isoformat()}] "
              f"{'✓ APPROVED' if approved else '✗ REJECTED'}: {reason} "
              f"(model: {data.get('model_name', 'unknown')})")
        
        return jsonify({
            "approved": approved,
            "reason": reason,
            "validator_id": VALIDATOR_ID,
            "timestamp": datetime.now().isoformat()
        })
        
    except Exception as e:
        stats["rejected"] += 1
        return jsonify({
            "approved": False,
            "reason": f"Server error: {str(e)}",
            "validator_id": VALIDATOR_ID,
            "timestamp": datetime.now().isoformat()
        }), 500


@app.route('/stats', methods=['GET'])
def get_stats():
    """Statistiques du validateur"""
    total = stats["total_requests"]
    if total > 0:
        approval_rate = (stats["approved"] / total) * 100
    else:
        approval_rate = 0
    
    return jsonify({
        **stats,
        "approval_rate": f"{approval_rate:.1f}%"
    })


if __name__ == '__main__':
    print("=" * 60)
    print(f"🛡️  DRC Network Consensus Validator Server")
    print(f"📍 Validator ID: {VALIDATOR_ID}")
    print(f"🔢 Version: {VALIDATOR_VERSION}")
    print(f"🇸🇳 Made in Senegal")
    print("=" * 60)
    print(f"\n✓ Health check: http://localhost:5000/health")
    print(f"✓ Validate endpoint: http://localhost:5000/validate (POST)")
    print(f"✓ Stats: http://localhost:5000/stats")
    print(f"\nStarting server...\n")
    
    # En production, utiliser gunicorn ou similaire
    app.run(host='0.0.0.0', port=5000, debug=True)
