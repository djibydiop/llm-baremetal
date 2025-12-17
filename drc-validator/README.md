# DRC Network Consensus - Serveurs Validators

## 🎯 Architecture

Système de validation de boot distribué:
- **3 serveurs validators** indépendants
- **Consensus 2/3** requis pour approuver un boot
- **Règles DRC-lite** embarquées dans chaque validator

## 🚀 Démarrage Rapide

### 1. Installation

```bash
cd drc-validator
pip install -r requirements.txt
```

### 2. Lancer le premier validator

```bash
python validator_server.py
```

Le serveur démarre sur `http://localhost:5000`

### 3. Tester le health check

```bash
curl http://localhost:5000/health
```

Réponse:
```json
{
  "status": "healthy",
  "validator_id": "validator-1",
  "version": "1.0.0",
  "uptime": 123.45
}
```

### 4. Tester une validation

```bash
curl -X POST http://localhost:5000/validate \
  -H "Content-Type: application/json" \
  -d '{
    "system_state": {
      "failed_boots": 0,
      "last_boot_time": "2025-12-16T10:00:00Z"
    },
    "model_name": "stories15M",
    "model_hash": "abc123456789",
    "drc_version": "5.1",
    "last_tokens": [1, 2, 4, 6, 8]
  }'
```

Réponse APPROVED:
```json
{
  "approved": true,
  "reason": "All DRC rules passed",
  "validator_id": "validator-1",
  "timestamp": "2025-12-16T10:00:01Z"
}
```

Réponse REJECTED (si token 3 détecté dans contexte dangereux):
```json
{
  "approved": false,
  "reason": "DRC: Token 3 in dangerous context detected",
  "validator_id": "validator-1",
  "timestamp": "2025-12-16T10:00:02Z"
}
```

## 🔧 Configuration Multi-Validators

Pour lancer 3 validators sur différents ports:

### Terminal 1 - Validator 1
```bash
VALIDATOR_ID=validator-1 python validator_server.py
# Port 5000
```

### Terminal 2 - Validator 2
```bash
VALIDATOR_ID=validator-2 FLASK_RUN_PORT=5001 python validator_server.py
# Port 5001
```

### Terminal 3 - Validator 3
```bash
VALIDATOR_ID=validator-3 FLASK_RUN_PORT=5002 python validator_server.py
# Port 5002
```

## 📊 Statistiques

Consulter les stats d'un validator:

```bash
curl http://localhost:5000/stats
```

Réponse:
```json
{
  "total_requests": 42,
  "approved": 38,
  "rejected": 4,
  "approval_rate": "90.5%",
  "start_time": "2025-12-16T10:00:00.000000"
}
```

## 🛡️ Règles DRC Implémentées

### Règle 1: Version DRC
- Vérifie que le client utilise DRC v5.1
- Rejette si version incompatible

### Règle 2: Intégrité du modèle
- Vérifie le hash du modèle (optionnel)
- Compare avec base de modèles approuvés

### Règle 3: Anti-spam
- Max 100 boots par heure
- Bloque si trop de tentatives échouées (>5)

### Règle 4: Token 3 Suppression
- Vérifie les derniers tokens générés
- Rejette si token 3 dans contexte dangereux

## 🌐 Déploiement Production

### Option 1: VPS (recommandé)

Sur un VPS Ubuntu/Debian:

```bash
# Installer Python
sudo apt update
sudo apt install python3 python3-pip

# Installer le validator
cd /opt
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal/drc-validator
pip3 install -r requirements.txt

# Lancer avec gunicorn (production)
pip3 install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 validator_server:app
```

### Option 2: Docker

```dockerfile
FROM python:3.11-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install -r requirements.txt

COPY validator_server.py .

EXPOSE 5000
CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "validator_server:app"]
```

Build & run:
```bash
docker build -t drc-validator .
docker run -p 5000:5000 drc-validator
```

### Option 3: Raspberry Pi (local)

Idéal pour développement:

```bash
# Sur le Pi
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal/drc-validator
pip3 install -r requirements.txt
python3 validator_server.py
```

## 🔒 Sécurité

### HTTPS (production)

Utiliser nginx en reverse proxy:

```nginx
server {
    listen 443 ssl;
    server_name validator1.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### Authentication (optionnel)

Ajouter un token d'authentification:

```python
API_TOKEN = "votre_token_secret"

@app.before_request
def check_auth():
    token = request.headers.get('Authorization')
    if token != f"Bearer {API_TOKEN}":
        return jsonify({"error": "Unauthorized"}), 401
```

## 📈 Monitoring

### Logs

Logs en temps réel:
```bash
tail -f validator.log
```

Format:
```
[2025-12-16T10:00:01] ✓ APPROVED: All DRC rules passed (model: stories15M)
[2025-12-16T10:00:05] ✗ REJECTED: DRC version mismatch (model: stories15M)
```

### Prometheus (avancé)

Ajouter des métriques Prometheus:

```python
from prometheus_client import Counter, Histogram, generate_latest

REQUEST_COUNT = Counter('validator_requests_total', 'Total requests')
APPROVAL_COUNT = Counter('validator_approvals_total', 'Approved boots')
REJECTION_COUNT = Counter('validator_rejections_total', 'Rejected boots')

@app.route('/metrics')
def metrics():
    return generate_latest()
```

## 🧪 Tests

Test unitaire basique:

```python
import requests

def test_validator():
    # Health check
    r = requests.get('http://localhost:5000/health')
    assert r.status_code == 200
    assert r.json()['status'] == 'healthy'
    
    # Valid request
    r = requests.post('http://localhost:5000/validate', json={
        "drc_version": "5.1",
        "model_name": "stories15M",
        "model_hash": "abc123",
        "last_tokens": [1, 2, 4, 6]
    })
    assert r.status_code == 200
    assert r.json()['approved'] == True
    
    # Invalid version
    r = requests.post('http://localhost:5000/validate', json={
        "drc_version": "4.0",
        "model_name": "stories15M"
    })
    assert r.json()['approved'] == False

if __name__ == '__main__':
    test_validator()
    print("✓ All tests passed")
```

## 🔄 Intégration Bare-Metal

Les URLs des validators seront configurées dans `llama2_efi.c`:

```c
const CHAR8* validator_urls[] = {
    "http://validator1.example.com/validate",
    "http://validator2.example.com/validate",
    "http://validator3.example.com/validate",
    NULL
};

// Consensus: au moins 2/3 doivent approuver
int approvals = 0;
for (int i = 0; validator_urls[i] != NULL; i++) {
    if (http_request_validator(validator_urls[i], system_state)) {
        approvals++;
    }
}

if (approvals >= 2) {
    // Boot autorisé
    boot_system();
} else {
    // Boot refusé
    Print(L"BOOT REJECTED by DRC consensus\n");
    halt();
}
```

## 📞 Support

- Issues: https://github.com/djibydiop/llm-baremetal/issues
- Docs: [ROADMAP_POST_BOOT.md](../ROADMAP_POST_BOOT.md)

---

**Made in Senegal 🇸🇳**
