#!/bin/bash
echo "Downloading Stories15M model and tokenizer..."
wget -O stories15M.bin https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
wget -O tokenizer.bin https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
echo "Done! Model and tokenizer downloaded."
