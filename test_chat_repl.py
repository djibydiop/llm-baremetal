#!/usr/bin/env python3
"""
Script de test Chat REPL pour llm-baremetal
Simule des conversations multi-tours pour tester la qualité
"""

test_conversations = [
    {
        "title": "Story Follow-up",
        "turns": [
            "Tell me a short story about a brave mouse.",
            "What did the mouse do in that story?",
            "Was the mouse scared?"
        ]
    },
    {
        "title": "Technical Q&A",
        "turns": [
            "How do computers work?",
            "What is the most important part?",
            "Can computers think like humans?"
        ]
    },
    {
        "title": "Creative Writing",
        "turns": [
            "Write a poem about stars.",
            "Now make it rhyme.",
            "Add a verse about the moon."
        ]
    },
    {
        "title": "Science Education",
        "turns": [
            "What is gravity?",
            "Why do objects fall down?",
            "Does gravity exist in space?"
        ]
    },
    {
        "title": "Philosophy",
        "turns": [
            "What is happiness?",
            "How can someone find happiness?",
            "Is happiness different for everyone?"
        ]
    }
]

print("=" * 70)
print("  LLM BARE-METAL - CHAT REPL TEST SCENARIOS")
print("=" * 70)
print()

for idx, conv in enumerate(test_conversations, 1):
    print(f"\n[CONVERSATION {idx}/{len(test_conversations)}] {conv['title']}")
    print("-" * 70)
    
    for turn_num, question in enumerate(conv['turns'], 1):
        print(f"\nTurn {turn_num}:")
        print(f"  User: {question}")
        print(f"  Assistant: [Model will generate response...]")
    
    print()

print("\n" + "=" * 70)
print("  EXPECTED IMPROVEMENTS")
print("=" * 70)
print("""
✅ Context Retention
   - Assistant remembers previous turns
   - Pronouns (it, that, the mouse) reference earlier mentions
   
✅ Coherent Multi-Turn Dialogue
   - Follow-up questions get relevant answers
   - Conversation flows naturally
   
✅ Quality Metrics
   - Temperature: 0.85 (balanced creativity/coherence)
   - Tokens per response: 64-128 (concise but complete)
   - Repetition penalty: 3.5x exponential (no loops)
   - Max history: 10 turns (context window)

✅ Chat Features
   - System prompt: "You are a helpful, friendly AI assistant..."
   - User/Assistant role formatting
   - Conversation summary at end
""")

print("\n" + "=" * 70)
print("  DEPLOYMENT")
print("=" * 70)
print("""
1. Compile: wsl make clean && wsl make
2. Test QEMU: .\\run-qemu.ps1
3. Create USB: .\\create-img-for-rufus.ps1 (as Admin)
4. Write with Rufus (DD mode)
5. Boot on real hardware!
""")
