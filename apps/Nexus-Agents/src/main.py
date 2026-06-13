"""Nexus Agents — Multi-agent system builder and orchestrator."""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import time
import uuid
from typing import Optional

app = FastAPI(title="Nexus Agents", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

STARTED_AT = time.time()
agents: dict[str, dict] = {}
tasks: dict[str, dict] = {}

class AgentCreate(BaseModel):
    name: str
    role: str = "assistant"
    provider: str = "auto"
    model: str = ""
    system_prompt: str = ""
    tools: list[str] = []

class TaskCreate(BaseModel):
    agent_id: str
    description: str
    context: dict = {}
    priority: int = 0

class TaskDelegate(BaseModel):
    from_agent_id: str
    to_agent_id: str
    task_id: str
    reason: str = ""

@app.get("/health")
async def health():
    return {"service": "nexus-agents", "status": "ok", "version": "v1", "uptime_seconds": int(time.time() - STARTED_AT)}

@app.get("/api/v1/agents/status")
async def status():
    return {"agents": len(agents), "tasks_total": len(tasks), "tasks_pending": sum(1 for t in tasks.values() if t["status"] == "pending"), "tasks_completed": sum(1 for t in tasks.values() if t["status"] == "completed")}

@app.post("/api/v1/agents")
async def create_agent(agent: AgentCreate):
    a = {"id": str(uuid.uuid4()), "name": agent.name, "role": agent.role, "provider": agent.provider, "model": agent.model, "system_prompt": agent.system_prompt, "tools": agent.tools, "status": "idle", "tasks_completed": 0, "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    agents[a["id"]] = a
    return {"agent": a}

@app.get("/api/v1/agents")
async def list_agents():
    return {"agents": list(agents.values())}

@app.get("/api/v1/agents/{agent_id}")
async def get_agent(agent_id: str):
    if agent_id not in agents: return {"error": "not found"}
    return {"agent": agents[agent_id]}

@app.delete("/api/v1/agents/{agent_id}")
async def delete_agent(agent_id: str):
    if agent_id not in agents: return {"error": "not found"}
    del agents[agent_id]
    return {"deleted": True}

@app.post("/api/v1/agents/{agent_id}/tasks")
async def assign_task(agent_id: str, task: TaskCreate):
    if agent_id not in agents: return {"error": "agent not found"}
    t = {"id": str(uuid.uuid4()), "agent_id": agent_id, "description": task.description, "context": task.context, "priority": task.priority, "status": "pending", "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "completed_at": None}
    tasks[t["id"]] = t
    return {"task": t}

@app.get("/api/v1/agents/{agent_id}/tasks")
async def list_tasks(agent_id: str):
    return {"tasks": [t for t in tasks.values() if t["agent_id"] == agent_id]}

@app.get("/api/v1/tasks/{task_id}")
async def get_task(task_id: str):
    if task_id not in tasks: return {"error": "not found"}
    return {"task": tasks[task_id]}

@app.post("/api/v1/tasks/{task_id}/complete")
async def complete_task(task_id: str, result: dict = {}):
    if task_id not in tasks: return {"error": "not found"}
    tasks[task_id]["status"] = "completed"
    tasks[task_id]["completed_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    tasks[task_id]["result"] = result.get("output", "")
    aid = tasks[task_id]["agent_id"]
    if aid in agents: agents[aid]["tasks_completed"] += 1
    return {"task": tasks[task_id]}

@app.post("/api/v1/tasks/{task_id}/delegate")
async def delegate_task(task_id: str, delegation: TaskDelegate):
    if task_id not in tasks: return {"error": "task not found"}
    if delegation.to_agent_id not in agents: return {"error": "target agent not found"}
    tasks[task_id]["agent_id"] = delegation.to_agent_id
    tasks[task_id]["delegated_from"] = delegation.from_agent_id
    tasks[task_id]["delegation_reason"] = delegation.reason
    return {"task": tasks[task_id], "delegated": True}
