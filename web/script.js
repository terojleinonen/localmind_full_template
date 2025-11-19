const chatBox = document.getElementById("chat-box");
const form = document.getElementById("chat-form");
const input = document.getElementById("user-input");

function addMessage(text, who) {
  const div = document.createElement("div");
  div.classList.add("message", who);
  div.textContent = text;
  chatBox.appendChild(div);
  chatBox.scrollTop = chatBox.scrollHeight;
}

async function askLocalMind(question) {
  addMessage(question, "user");
  addMessage("Thinking (stub backend)...", "ai");

  try {
    const res = await fetch("http://localhost:8080/query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ question }),
    });
    const data = await res.json();
    const last = chatBox.querySelector(".message.ai:last-child");
    last.textContent = data.answer || "No answer (stub).";
  } catch (e) {
    const last = chatBox.querySelector(".message.ai:last-child");
    last.textContent = "⚠️ Could not reach backend.";
  }
}

form.addEventListener("submit", (e) => {
  e.preventDefault();
  const text = input.value.trim();
  if (!text) return;
  input.value = "";
  askLocalMind(text);
});
