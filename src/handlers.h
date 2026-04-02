#pragma once
#include "web_context.h"

void handleCreate(WebContext& ctx);
void handleRead(WebContext& ctx);
void handleDelete(WebContext& ctx);
void handleUpdate(WebContext& ctx);
void handleSpace(WebContext& ctx);
void handleBulkCreate(WebContext& ctx);
void sendAuthCORS(WebContext& ctx);
