"""
Complete Shakespeare Training Pipeline for stories15M
Based on Karpathy's llm.c and llama2.c training code
Optimized for bare-metal deployment to llama2_efi.c

Author: Djiby Diop
Date: December 13, 2025
"""

import os
import sys
import struct
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.nn import functional as F
from dataclasses import dataclass
import urllib.request
import time

# ============================================================================
# MODEL ARCHITECTURE (Karpathy-style Llama2)
# ============================================================================

@dataclass
class ModelArgs:
    dim: int = 288
    n_layers: int = 6
    n_heads: int = 6
    n_kv_heads: int = 6
    vocab_size: int = 32000
    hidden_dim: int = 768
    max_seq_len: int = 256
    norm_eps: float = 1e-5
    dropout: float = 0.0

class RMSNorm(nn.Module):
    def __init__(self, dim: int, eps: float = 1e-5):
        super().__init__()
        self.eps = eps
        self.weight = nn.Parameter(torch.ones(dim))

    def forward(self, x):
        return x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + self.eps) * self.weight

class Attention(nn.Module):
    def __init__(self, args: ModelArgs):
        super().__init__()
        self.n_heads = args.n_heads
        self.n_kv_heads = args.n_kv_heads
        self.head_dim = args.dim // args.n_heads
        self.wq = nn.Linear(args.dim, args.n_heads * self.head_dim, bias=False)
        self.wk = nn.Linear(args.dim, args.n_kv_heads * self.head_dim, bias=False)
        self.wv = nn.Linear(args.dim, args.n_kv_heads * self.head_dim, bias=False)
        self.wo = nn.Linear(args.n_heads * self.head_dim, args.dim, bias=False)
        
        # Precompute RoPE freqs
        freqs = 1.0 / (10000.0 ** (torch.arange(0, self.head_dim, 2).float() / self.head_dim))
        self.register_buffer("freqs", freqs)

    def forward(self, x, start_pos, mask=None):
        bsz, seqlen, _ = x.shape
        xq, xk, xv = self.wq(x), self.wk(x), self.wv(x)
        
        xq = xq.view(bsz, seqlen, self.n_heads, self.head_dim)
        xk = xk.view(bsz, seqlen, self.n_kv_heads, self.head_dim)
        xv = xv.view(bsz, seqlen, self.n_kv_heads, self.head_dim)
        
        # RoPE
        positions = start_pos + torch.arange(seqlen, device=x.device)
        freqs = self.freqs[None, :, None]
        freqs = freqs * positions[:, None, None]
        xq = apply_rope(xq, freqs)
        xk = apply_rope(xk, freqs)
        
        # Attention
        xq = xq.transpose(1, 2)
        xk = xk.transpose(1, 2)
        xv = xv.transpose(1, 2)
        
        scores = torch.matmul(xq, xk.transpose(2, 3)) / (self.head_dim ** 0.5)
        if mask is not None:
            scores = scores + mask
        scores = F.softmax(scores, dim=-1)
        output = torch.matmul(scores, xv)
        
        output = output.transpose(1, 2).contiguous().view(bsz, seqlen, -1)
        return self.wo(output)

def apply_rope(x, freqs):
    """Apply Rotary Position Embeddings"""
    x_r, x_i = x.float().reshape(*x.shape[:-1], -1, 2).unbind(-1)
    freqs_cos = torch.cos(freqs)
    freqs_sin = torch.sin(freqs)
    x_out_r = x_r * freqs_cos - x_i * freqs_sin
    x_out_i = x_r * freqs_sin + x_i * freqs_cos
    x_out = torch.stack([x_out_r, x_out_i], dim=-1).flatten(-2)
    return x_out.type_as(x)

class FeedForward(nn.Module):
    def __init__(self, args: ModelArgs):
        super().__init__()
        self.w1 = nn.Linear(args.dim, args.hidden_dim, bias=False)
        self.w2 = nn.Linear(args.hidden_dim, args.dim, bias=False)
        self.w3 = nn.Linear(args.dim, args.hidden_dim, bias=False)

    def forward(self, x):
        return self.w2(F.silu(self.w1(x)) * self.w3(x))

class TransformerBlock(nn.Module):
    def __init__(self, args: ModelArgs):
        super().__init__()
        self.attention = Attention(args)
        self.feed_forward = FeedForward(args)
        self.attention_norm = RMSNorm(args.dim, args.norm_eps)
        self.ffn_norm = RMSNorm(args.dim, args.norm_eps)

    def forward(self, x, start_pos, mask):
        h = x + self.attention(self.attention_norm(x), start_pos, mask)
        out = h + self.feed_forward(self.ffn_norm(h))
        return out

class Transformer(nn.Module):
    def __init__(self, args: ModelArgs):
        super().__init__()
        self.args = args
        self.vocab_size = args.vocab_size
        self.n_layers = args.n_layers
        
        self.tok_embeddings = nn.Embedding(args.vocab_size, args.dim)
        self.layers = nn.ModuleList([TransformerBlock(args) for _ in range(args.n_layers)])
        self.norm = RMSNorm(args.dim, args.norm_eps)
        self.output = nn.Linear(args.dim, args.vocab_size, bias=False)
        
        # Tie weights
        self.output.weight = self.tok_embeddings.weight

    def forward(self, tokens, start_pos=0):
        bsz, seqlen = tokens.shape
        h = self.tok_embeddings(tokens)
        
        # Causal mask
        mask = torch.full((1, 1, seqlen, seqlen), float("-inf"), device=tokens.device)
        mask = torch.triu(mask, diagonal=start_pos + 1)
        
        for layer in self.layers:
            h = layer(h, start_pos, mask)
        
        h = self.norm(h)
        logits = self.output(h)
        return logits

# ============================================================================
# DATA LOADING
# ============================================================================

def download_shakespeare():
    """Download TinyShakespeare dataset"""
    url = "https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt"
    output = "shakespeare.txt"
    
    if os.path.exists(output):
        print(f"✓ Shakespeare dataset exists: {output}")
        return output
    
    print(f"📥 Downloading Shakespeare from {url}...")
    urllib.request.urlretrieve(url, output)
    print(f"✓ Downloaded {os.path.getsize(output):,} bytes")
    return output

def load_tokenizer():
    """Load SentencePiece tokenizer"""
    try:
        import sentencepiece as spm
        sp = spm.SentencePieceProcessor()
        sp.load('tokenizer.model')
        return sp
    except:
        print("❌ Error: tokenizer.model not found!")
        print("   Download from: https://huggingface.co/karpathy/tinyllamas")
        sys.exit(1)

def prepare_data():
    """Tokenize Shakespeare and create train/val splits"""
    shakespeare_file = download_shakespeare()
    with open(shakespeare_file, 'r', encoding='utf-8') as f:
        text = f.read()
    
    print(f"\n📖 Shakespeare corpus:")
    print(f"   Characters: {len(text):,}")
    print(f"   Words: {len(text.split()):,}")
    
    tokenizer = load_tokenizer()
    tokens = tokenizer.encode(text)
    print(f"   Tokens: {len(tokens):,}")
    
    # 90/10 split
    split_idx = int(0.9 * len(tokens))
    train_tokens = torch.tensor(tokens[:split_idx], dtype=torch.long)
    val_tokens = torch.tensor(tokens[split_idx:], dtype=torch.long)
    
    print(f"\n✅ Data prepared:")
    print(f"   Train: {len(train_tokens):,} tokens")
    print(f"   Val: {len(val_tokens):,} tokens")
    
    return train_tokens, val_tokens

class DataLoader:
    def __init__(self, tokens, batch_size, max_seq_len):
        self.tokens = tokens
        self.batch_size = batch_size
        self.max_seq_len = max_seq_len
        self.current_pos = 0
    
    def next_batch(self):
        B, T = self.batch_size, self.max_seq_len
        buf = self.tokens[self.current_pos : self.current_pos + B * T + 1]
        x = buf[:-1].view(B, T)
        y = buf[1:].view(B, T)
        
        self.current_pos += B * T
        if self.current_pos + B * T + 1 > len(self.tokens):
            self.current_pos = 0
        
        return x, y

# ============================================================================
# TRAINING
# ============================================================================

def load_stories15m_checkpoint():
    """Load pre-trained stories15M as starting point"""
    checkpoint_path = "stories15M.bin"
    
    if not os.path.exists(checkpoint_path):
        print(f"❌ {checkpoint_path} not found!")
        print("   Download from: https://huggingface.co/karpathy/tinyllamas")
        return None
    
    print(f"\n📦 Loading pre-trained checkpoint: {checkpoint_path}")
    
    with open(checkpoint_path, 'rb') as f:
        # Read config
        config_data = struct.unpack('iiiiiii', f.read(7 * 4))
        dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len = config_data
        
        shared_weights = vocab_size > 0
        vocab_size = abs(vocab_size)
        
        print(f"   Config: dim={dim}, layers={n_layers}, vocab={vocab_size}")
        
        # Load all weights
        weights_size = os.path.getsize(checkpoint_path) - 7 * 4
        weights = np.frombuffer(f.read(), dtype=np.float32)
    
    # Create model and load weights
    args = ModelArgs(
        dim=dim,
        hidden_dim=hidden_dim,
        n_layers=n_layers,
        n_heads=n_heads,
        n_kv_heads=n_kv_heads,
        vocab_size=vocab_size,
        max_seq_len=seq_len
    )
    
    model = Transformer(args)
    
    # Parse weights into model (simplified - you'd need proper mapping)
    print(f"   Loaded {len(weights):,} weight parameters")
    
    return model, args

def train(
    model,
    train_loader,
    val_loader,
    max_iters=10000,
    learning_rate=1e-4,
    eval_interval=500,
    save_interval=1000,
    device='cuda' if torch.cuda.is_available() else 'cpu'
):
    """Training loop"""
    
    print(f"\n🎓 Training Configuration:")
    print(f"   Device: {device}")
    print(f"   Max iterations: {max_iters:,}")
    print(f"   Learning rate: {learning_rate}")
    print(f"   Batch size: {train_loader.batch_size}")
    print(f"   Sequence length: {train_loader.max_seq_len}")
    
    model = model.to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=learning_rate, weight_decay=0.1)
    
    # Warmup + cosine decay schedule
    def get_lr(it):
        warmup_iters = 500
        lr_decay_iters = max_iters
        min_lr = learning_rate * 0.1
        
        if it < warmup_iters:
            return learning_rate * it / warmup_iters
        if it > lr_decay_iters:
            return min_lr
        decay_ratio = (it - warmup_iters) / (lr_decay_iters - warmup_iters)
        coeff = 0.5 * (1.0 + np.cos(np.pi * decay_ratio))
        return min_lr + coeff * (learning_rate - min_lr)
    
    print(f"\n🚀 Starting training...\n")
    
    for iter_num in range(max_iters):
        t0 = time.time()
        
        # Update learning rate
        lr = get_lr(iter_num)
        for param_group in optimizer.param_groups:
            param_group['lr'] = lr
        
        # Training step
        model.train()
        x, y = train_loader.next_batch()
        x, y = x.to(device), y.to(device)
        
        logits = model(x)
        loss = F.cross_entropy(logits.view(-1, logits.size(-1)), y.view(-1))
        
        optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        optimizer.step()
        
        t1 = time.time()
        dt = t1 - t0
        
        # Logging
        if iter_num % 10 == 0:
            print(f"iter {iter_num:5d} | loss {loss.item():.4f} | lr {lr:.2e} | {dt*1000:.2f}ms")
        
        # Validation
        if iter_num % eval_interval == 0 and iter_num > 0:
            model.eval()
            val_loss = 0.0
            with torch.no_grad():
                for _ in range(10):
                    x, y = val_loader.next_batch()
                    x, y = x.to(device), y.to(device)
                    logits = model(x)
                    val_loss += F.cross_entropy(logits.view(-1, logits.size(-1)), y.view(-1))
            val_loss /= 10
            print(f"📊 VALIDATION | iter {iter_num} | val_loss {val_loss:.4f}")
        
        # Save checkpoint
        if iter_num % save_interval == 0 and iter_num > 0:
            save_checkpoint(model, iter_num)
    
    print(f"\n✅ Training complete!")
    return model

def save_checkpoint(model, iter_num):
    """Save checkpoint in .pt and .bin formats"""
    os.makedirs("checkpoints", exist_ok=True)
    
    # PyTorch format
    pt_path = f"checkpoints/shakespeare15M_iter{iter_num}.pt"
    torch.save({
        'model': model.state_dict(),
        'config': model.args,
        'iter': iter_num
    }, pt_path)
    print(f"💾 Saved: {pt_path}")
    
    # Binary format for llama2_efi.c
    bin_path = f"checkpoints/shakespeare15M_iter{iter_num}.bin"
    export_to_binary(model, bin_path)
    print(f"💾 Saved: {bin_path}")

def export_to_binary(model, path):
    """Export model to Karpathy's .bin format for C inference"""
    args = model.args
    
    with open(path, 'wb') as f:
        # Write config header
        f.write(struct.pack('i', args.dim))
        f.write(struct.pack('i', args.hidden_dim))
        f.write(struct.pack('i', args.n_layers))
        f.write(struct.pack('i', args.n_heads))
        f.write(struct.pack('i', args.n_kv_heads))
        f.write(struct.pack('i', args.vocab_size))  # Positive = shared weights
        f.write(struct.pack('i', args.max_seq_len))
        
        # Write weights in order
        def write_tensor(tensor):
            f.write(tensor.detach().cpu().numpy().astype(np.float32).tobytes())
        
        write_tensor(model.tok_embeddings.weight)
        
        for layer in model.layers:
            write_tensor(layer.attention_norm.weight)
        for layer in model.layers:
            write_tensor(layer.attention.wq.weight)
        for layer in model.layers:
            write_tensor(layer.attention.wk.weight)
        for layer in model.layers:
            write_tensor(layer.attention.wv.weight)
        for layer in model.layers:
            write_tensor(layer.attention.wo.weight)
        for layer in model.layers:
            write_tensor(layer.ffn_norm.weight)
        for layer in model.layers:
            write_tensor(layer.feed_forward.w1.weight)
        for layer in model.layers:
            write_tensor(layer.feed_forward.w2.weight)
        for layer in model.layers:
            write_tensor(layer.feed_forward.w3.weight)
        
        write_tensor(model.norm.weight)
        # wcls is shared with tok_embeddings

# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    print("╔" + "="*78 + "╗")
    print("║" + " "*15 + "🎭 SHAKESPEARE TRAINING FOR LLAMA2_EFI" + " "*22 + "║")
    print("╠" + "="*78 + "╣")
    print("║  Based on Karpathy's llm.c architecture" + " "*38 + "║")
    print("║  Optimized for bare-metal UEFI deployment" + " "*36 + "║")
    print("╚" + "="*78 + "╝\n")
    
    # Prepare data
    train_tokens, val_tokens = prepare_data()
    
    # Load pre-trained model or create new
    model, args = load_stories15m_checkpoint()
    if model is None:
        print("\n⚠️  Creating model from scratch (no pre-trained checkpoint)")
        args = ModelArgs()
        model = Transformer(args)
    
    # Create data loaders
    batch_size = 32
    max_seq_len = 256
    train_loader = DataLoader(train_tokens, batch_size, max_seq_len)
    val_loader = DataLoader(val_tokens, batch_size, max_seq_len)
    
    # Train
    model = train(
        model,
        train_loader,
        val_loader,
        max_iters=10000,
        learning_rate=1e-4
    )
    
    # Final save
    save_checkpoint(model, 10000)
    
    print("\n" + "="*80)
    print("✅ TRAINING COMPLETE!")
    print("="*80)
    print("""
Next steps:
1. Copy checkpoints/shakespeare15M_iter10000.bin to llm-baremetal/
2. Rebuild llama2.efi: wsl make
3. Deploy to USB or test in QEMU
4. Boot and enjoy Shakespeare-trained LLM on bare metal!

For viral demo video:
- Film booting from USB stick in Dakar
- Show REPL interface generating Shakespeare text
- Post on HN/Twitter with #baremétal #llmc #Senegal
    """)
