#!/usr/bin/env python3
"""End-to-end API integration tests for the Instagram clone backend.

Requires the backend to be running (default http://127.0.0.1:8080).
Uses only the Python standard library so it runs without pip installs.

    python tests/test_api.py
    IG_API_URL=http://127.0.0.1:8080 python tests/test_api.py
"""
from __future__ import annotations

import base64
import json
import os
import sys
import time
import traceback
import urllib.error
import urllib.request
from typing import Any, Dict, Optional, Tuple

BASE = os.environ.get("IG_API_URL", "http://127.0.0.1:8080").rstrip("/")
PNG = ("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8"
       "z8DwHwAFAAH/q842iQAAAABJRU5ErkJggg==")

checks = 0


def call(method: str, path: str, body: Optional[Dict[str, Any]] = None,
         token: Optional[str] = None) -> Tuple[int, Dict[str, Any]]:
    global checks
    checks += 1
    data = json.dumps(body).encode("utf-8") if body is not None else None
    headers = {"Accept": "application/json"}
    if data is not None:
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = "Bearer " + token
    request = urllib.request.Request(BASE + path, data=data, headers=headers,
                                     method=method)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            raw = response.read().decode("utf-8")
            status = response.status
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8")
        status = exc.code
    try:
        parsed = json.loads(raw) if raw else {}
    except json.JSONDecodeError:
        parsed = {"raw": raw}
    return status, parsed


def expect(status: int, result: Tuple[int, Dict[str, Any]], label: str) -> Dict[str, Any]:
    actual, body = result
    if actual != status:
        raise AssertionError(f"{label}: expected {status}, got {actual}: {body}")
    return body


def main() -> int:
    suffix = str(int(time.time()))
    alice = "testa" + suffix[-7:]
    bob = "testb" + suffix[-7:]
    password = "Test@12345"

    body = expect(200, call("GET", "/api/health"), "health")
    assert body.get("status") == "ok"

    body = expect(401, call("GET", "/api/auth/me"), "protected route")
    assert "error" in body

    body = expect(201, call("POST", "/api/auth/register", {
        "username": alice, "email": f"{alice}@example.com",
        "password": password, "fullName": "API Test Alice"}), "register alice")
    token_a = body["token"]
    user_a = body["user"]
    assert user_a["username"] == alice

    body = expect(201, call("POST", "/api/auth/register", {
        "username": bob, "email": f"{bob}@example.com",
        "password": password, "fullName": "API Test Bob"}), "register bob")
    token_b = body["token"]
    user_b = body["user"]

    expect(409, call("POST", "/api/auth/register", {
        "username": alice, "email": f"{alice}@example.com",
        "password": password}), "duplicate registration")

    body = expect(200, call("POST", "/api/auth/login", {
        "username": alice, "password": password}), "login")
    token_a = body["token"]
    expect(401, call("POST", "/api/auth/login", {
        "username": alice, "password": "WrongPassword!"}), "bad login")

    body = expect(200, call("GET", "/api/auth/me", token=token_a), "me")
    assert body["id"] == user_a["id"]

    expect(200, call("PUT", f"/api/users/{user_a['id']}", {
        "bio": "Updated by integration test", "website": "https://example.com",
        "fullName": "Renamed Alice"}, token_a), "update profile")

    body = expect(200, call("GET", f"/api/users/{user_b['id']}", token=token_a),
                  "user profile")
    assert body["postsCount"] == 0

    expect(200, call("POST", f"/api/users/{user_b['id']}/follow", token=token_a),
           "follow")
    body = expect(200, call("GET", f"/api/users/{user_b['id']}/followers",
                            token=token_a), "followers")
    assert any(u["id"] == user_a["id"] for u in body["users"])

    body = expect(200, call("GET", f"/api/users/{user_a['id']}/following",
                            token=token_a), "following")
    assert any(u["id"] == user_b["id"] for u in body["users"])

    body = expect(200, call("GET", f"/api/users/{user_b['id']}/mutual",
                            token=token_a), "mutual followers")
    assert isinstance(body["users"], list)

    body = expect(200, call("GET", "/api/users/search?q=" + bob[:6],
                            token=token_a), "user search (trie)")
    assert any(u["id"] == user_b["id"] for u in body["results"])

    body = expect(200, call("GET", "/api/users/suggested", token=token_a),
                  "suggested users")
    assert isinstance(body["users"], list)

    body = expect(201, call("POST", "/api/posts", {
        "caption": "Integration test post #testing #api",
        "location": "Test Lab",
        "media": [{"data": PNG, "filename": "pixel.png", "type": "image"}],
    }, token_a), "create post")
    post = body["post"]
    post_id = post["id"]
    assert post["media"][0]["url"].startswith("/uploads/")
    assert "testing" in post["hashtags"]

    body = expect(200, call("GET", f"/api/posts/{post_id}", token=token_b),
                  "get post")
    assert body["post"]["caption"].startswith("Integration test")

    body = expect(200, call("GET", "/api/feed?mode=following", token=token_a),
                  "following feed")
    assert any(p["id"] == post_id for p in body["posts"])

    body = expect(200, call("GET", "/api/feed?mode=popular", token=token_b),
                  "popular feed")
    assert isinstance(body["posts"], list)

    expect(200, call("POST", f"/api/posts/{post_id}/like", token=token_b),
           "like post")
    body = expect(200, call("GET", f"/api/posts/{post_id}", token=token_b),
                  "liked post state")
    assert body["post"]["likeCount"] >= 1

    expect(200, call("DELETE", f"/api/posts/{post_id}/like", token=token_b),
           "unlike post")
    expect(200, call("POST", f"/api/posts/{post_id}/save", token=token_b),
           "save post")
    body = expect(200, call("GET", "/api/posts/saved", token=token_b),
                  "saved posts")
    assert any(p["id"] == post_id for p in body["posts"])
    expect(200, call("DELETE", f"/api/posts/{post_id}/save", token=token_b),
           "unsave post")

    body = expect(201, call("POST", f"/api/posts/{post_id}/comments", {
        "body": "Nice test post!"}, token_b), "add comment")
    assert body["comment"]["body"] == "Nice test post!"
    comment_id = body["comment"]["id"]
    body = expect(200, call("GET", f"/api/posts/{post_id}/comments",
                            token=token_a), "comments")
    assert any(c["id"] == comment_id for c in body["comments"])
    expect(200, call("DELETE", f"/api/comments/{comment_id}", token=token_b),
           "delete comment")

    body = expect(200, call("GET", "/api/explore?tag=testing", token=token_a),
                  "explore by hashtag")
    assert any(p["id"] == post_id for p in body["posts"])

    body = expect(200, call("GET", "/api/hashtags/trending", token=token_a),
                  "trending hashtags")
    assert any(h["tag"] == "testing" for h in body["hashtags"])

    body = expect(201, call("POST", "/api/stories", {
        "data": PNG, "filename": "story.png", "type": "image"}, token_a),
        "create story")
    assert body["id"] > 0
    body = expect(200, call("GET", "/api/stories", token=token_a), "stories")
    assert isinstance(body["stories"], list) and isinstance(body["mine"], list)

    body = expect(200, call("GET", "/api/reels?mode=trending", token=token_a),
                  "reels")
    assert isinstance(body["reels"], list)
    if body["reels"]:
        reel_id = body["reels"][0]["id"]
        expect(200, call("POST", f"/api/reels/{reel_id}/view", token=token_a),
               "view reel")
        expect(200, call("POST", f"/api/reels/{reel_id}/like", token=token_a),
               "like reel")
        expect(200, call("POST", f"/api/reels/{reel_id}/save", token=token_a),
               "save reel")
        body = expect(200, call("GET", "/api/reels/saved", token=token_a),
                      "saved reels")
        assert any(r["id"] == reel_id for r in body["reels"])
        expect(201, call("POST", f"/api/reels/{reel_id}/comments", {
            "body": "Great reel!"}, token=token_a), "reel comment")
        body = expect(200, call("GET", f"/api/reels/{reel_id}/comments",
                                token=token_a), "reel comments")
        assert any(c["body"] == "Great reel!" for c in body["comments"])

    body = expect(200, call("POST", f"/api/messages/conversation/{user_b['id']}", {
        "recipientId": user_b["id"]}, token_a), "create conversation")
    conversation_id = body["conversationId"]
    body = expect(200, call("GET", "/api/messages/conversations", token=token_a),
                  "conversations")
    assert any(c["id"] == conversation_id for c in body["conversations"])

    body = expect(201, call("POST", "/api/messages", {
        "conversationId": conversation_id, "body": "Hello from the API test"},
        token_a), "send message")
    message_id = body["message"]["id"]
    body = expect(200, call("GET", f"/api/messages/{conversation_id}",
                            token=token_b), "get messages")
    assert any(m["id"] == message_id for m in body["messages"])
    expect(200, call("POST", f"/api/messages/{conversation_id}/typing", {
        "typing": True}, token_b), "typing indicator")
    expect(200, call("POST", f"/api/messages/{conversation_id}/read",
                     token=token_b), "mark read")

    body = expect(200, call("GET", "/api/notifications", token=token_b),
                  "notifications")
    assert isinstance(body["notifications"], list)
    expect(200, call("GET", "/api/notifications/unread", token=token_b),
           "unread notifications")
    expect(200, call("POST", "/api/notifications/read-all", token=token_b),
           "read all notifications")

    body = expect(200, call("GET", "/api/search?q=test", token=token_a),
                  "global search")
    assert "users" in body and "hashtags" in body and "posts" in body
    body = expect(200, call("GET", "/api/search/recent", token=token_a),
                  "recent searches")
    assert isinstance(body["queries"], list)

    body = expect(200, call("POST", "/api/auth/forgot-password", {
        "email": f"{bob}@example.com"}, token=None), "forgot password")
    reset_token = body["devToken"]
    new_password = "NewPass@12345"
    expect(200, call("POST", "/api/auth/reset-password", {
        "token": reset_token, "password": new_password}), "reset password")
    expect(200, call("POST", "/api/auth/login", {
        "username": bob, "password": new_password}), "login after reset")

    expect(200, call("POST", "/api/auth/logout", token=token_a), "logout")
    expect(401, call("GET", "/api/auth/me", token=token_a),
           "token invalid after logout")

    print(f"API integration tests passed ({checks} requests)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # noqa: BLE001 - test runner reports all failures
        print(f"FAILED: {exc}", file=sys.stderr)
        traceback.print_exc()
        sys.exit(1)
