#ifndef ADMIN_H
#define ADMIN_H

#include <Arduino.h>

const char admin_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kitchen Master Dashboard</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --glass-border: rgba(255, 255, 255, 0.1);
            --accent: #38bdf8;
            --danger: #ef4444;
            --success: #22c55e;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); min-height: 100vh; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .container { width: 100%; max-width: 800px; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; }
        h1 { font-size: 1.6rem; color: #38bdf8; }
        a { color: var(--accent); text-decoration: none; font-size: 0.9rem; font-weight: 600; }
        .card { background: var(--card-bg); backdrop-filter: blur(12px); border: 1px solid var(--glass-border); border-radius: 16px; padding: 20px; margin-bottom: 20px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37); }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        @media(max-width: 600px) { .grid { grid-template-columns: 1fr; } }
        .order-card { background: rgba(15, 23, 42, 0.6); border: 1px solid var(--glass-border); border-left: 4px solid var(--accent); padding: 15px; border-radius: 10px; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
        .order-info h4 { font-size: 1.05rem; margin-bottom: 4px; }
        .order-info p { font-size: 0.85rem; color: var(--text-muted); }
        button { background: var(--success); color: white; border: none; padding: 8px 14px; border-radius: 6px; font-weight: 600; cursor: pointer; }
        button.danger { background: var(--danger); }
        button:hover { opacity: 0.9; }
        .form-inline { display: flex; gap: 8px; margin-top: 10px; }
        input { flex: 1; padding: 10px; border-radius: 8px; border: 1px solid var(--glass-border); background: #0f172a; color: white; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Kitchen Master Admin</h1>
            <a href="/">&larr; Customer View</a>
        </header>
        <div class="grid">
            <div class="card" style="grid-column: span 2;">
                <h2 style="margin-bottom: 15px; font-size: 1.2rem;">Live Incoming Orders</h2>
                <div id="orders-list">No active orders right now.</div>
            </div>
            <div class="card" style="grid-column: span 2;">
                <h2 style="margin-bottom: 15px; font-size: 1.2rem;">Manage Menu Items</h2>
                <div id="admin-menu-list">Loading menu...</div>
                <div style="margin-top: 15px; border-top: 1px solid var(--glass-border); padding-top: 15px;">
                    <h3 style="font-size: 1rem; margin-bottom: 8px;">Add New Food Item</h3>
                    <div class="form-inline">
                        <input type="text" id="new-name" placeholder="Food Name">
                        <input type="number" id="new-price" placeholder="Price ($)" step="0.1" style="max-width: 120px;">
                        <button onclick="addFood()">Add Item</button>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <script>
        async function fetchDashboardData() {
            let oRes = await fetch('/api/orders');
            let orders = await oRes.json();
            let oHtml = '';
            if(orders.length === 0) {
                oHtml = '<p style="color: var(--text-muted);">No active orders.</p>';
            } else {
                orders.forEach(o => {
                    oHtml += '<div class="order-card"><div class="order-info"><h4>' + o.foodName + '</h4><p>Customer: <strong>' + o.customerName + '</strong> | Seat: <strong>' + o.seatNumber + '</strong></p></div><button onclick="completeOrder(' + o.id + ')">Mark Ready</button></div>';
                });
            }
            document.getElementById('orders-list').innerHTML = oHtml;
            let mRes = await fetch('/api/menu');
            let menu = await mRes.json();
            let mHtml = '';
            menu.forEach(item => {
                mHtml += '<div style="display: flex; justify-content: space-between; align-items: center; padding: 8px 0; border-bottom: 1px solid var(--glass-border);"><span>' + item.name + ' - <strong>$' + item.price.toFixed(2) + '</strong></span><button class="danger" onclick="deleteFood(' + item.id + ')">Remove</button></div>';
            });
            document.getElementById('admin-menu-list').innerHTML = mHtml;
        }
        async function completeOrder(id) {
            await fetch('/api/order/complete?id=' + id, {method: 'POST'});
            fetchDashboardData();
        }
        async function addFood() {
            let name = document.getElementById('new-name').value;
            let price = document.getElementById('new-price').value;
            if(!name || !price) return alert('Fill all fields');
            await fetch('/api/menu/add', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({name: name, price: parseFloat(price)})
            });
            document.getElementById('new-name').value = '';
            document.getElementById('new-price').value = '';
            fetchDashboardData();
        }
        async function deleteFood(id) {
            await fetch('/api/menu/delete?id=' + id, {method: 'POST'});
            fetchDashboardData();
        }
        setInterval(fetchDashboardData, 2000);
        fetchDashboardData();
    </script>
</body>
</html>
)===";

#endif
