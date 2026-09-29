#ifndef INDEX_H
#define INDEX_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart IoT Digital Menu</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --glass-border: rgba(255, 255, 255, 0.1);
            --accent: #38bdf8;
            --accent-hover: #0ea5e9;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --success: #22c55e;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); min-height: 100vh; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .container { width: 100%; max-width: 600px; }
        header { text-align: center; margin-bottom: 24px; }
        h1 { font-size: 1.8rem; background: linear-gradient(to right, #38bdf8, #818cf8); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        .nav-links { margin-top: 10px; }
        .nav-links a { color: var(--accent); text-decoration: none; font-size: 0.9rem; font-weight: 600; }
        .card { background: var(--card-bg); backdrop-filter: blur(12px); border: 1px solid var(--glass-border); border-radius: 16px; padding: 20px; margin-bottom: 16px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37); }
        .menu-item { display: flex; justify-content: space-between; align-items: center; padding: 14px 0; border-bottom: 1px solid var(--glass-border); }
        .menu-item:last-child { border-bottom: none; }
        .item-info h3 { font-size: 1.1rem; margin-bottom: 4px; }
        .item-info p { color: var(--accent); font-weight: 600; }
        button { background: var(--accent); color: #0f172a; border: none; padding: 10px 18px; border-radius: 8px; font-weight: 700; cursor: pointer; transition: all 0.2s ease; }
        button:hover { background: var(--accent-hover); transform: translateY(-1px); }
        .modal { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.7); backdrop-filter: blur(5px); justify-content: center; align-items: center; z-index: 100; padding: 20px; }
        .modal-content { background: #1e293b; border: 1px solid var(--glass-border); border-radius: 16px; width: 100%; max-width: 400px; padding: 24px; }
        .form-group { margin-bottom: 16px; }
        label { display: block; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 6px; }
        input { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid var(--glass-border); background: #0f172a; color: white; font-size: 1rem; }
        input:focus { outline: none; border-color: var(--accent); }
        #status-box { display: none; text-align: center; padding: 20px; background: rgba(14, 165, 233, 0.15); border: 1px solid var(--accent); border-radius: 12px; margin-top: 20px; }
        .spinner { width: 30px; height: 30px; border: 3px solid rgba(255,255,255,0.3); border-radius: 50%; border-top-color: var(--accent); animation: spin 1s ease-in-out infinite; margin: 15px auto; }
        @keyframes spin { to { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Digital Menu & Ordering</h1>
            <div class="nav-links"><a href="/admin">Switch to Kitchen Admin Panel &rarr;</a></div>
        </header>
        <div class="card">
            <h2 style="margin-bottom: 15px; font-size: 1.2rem;">Available Menu</h2>
            <div id="menu-list">Loading menu...</div>
        </div>
        <div id="status-box" class="card">
            <h3>Order Pending Confirmation</h3>
            <div class="spinner"></div>
            <p id="status-text">Please touch the physical ESP32 sensor to confirm your order.</p>
        </div>
    </div>
    <div id="order-modal" class="modal">
        <div class="modal-content">
            <h3 id="modal-food-title" style="margin-bottom: 15px;">Complete Order</h3>
            <input type="hidden" id="selected-food">
            <div class="form-group">
                <label>Your Name</label>
                <input type="text" id="customer-name" placeholder="e.g., Alex Smith">
            </div>
            <div class="form-group">
                <label>Seat Number</label>
                <input type="text" id="seat-number" placeholder="e.g., Table 4 / Seat B">
            </div>
            <div style="display: flex; gap: 10px; margin-top: 20px;">
                <button style="flex: 1; background: #334155; color: white;" onclick="closeModal()">Cancel</button>
                <button style="flex: 1;" onclick="submitOrder()">Proceed to Touch</button>
            </div>
        </div>
    </div>
    <script>
        let checkInterval = null;
        async function loadMenu() {
            let res = await fetch('/api/menu');
            let items = await res.json();
            let html = '';
            items.forEach(item => {
                html += '<div class="menu-item"><div class="item-info"><h3>' + item.name + '</h3><p>$' + item.price.toFixed(2) + '</p></div><button onclick="openModal(\'' + item.name + '\')">Select</button></div>';
            });
            document.getElementById('menu-list').innerHTML = html;
        }
        function openModal(foodName) {
            document.getElementById('selected-food').value = foodName;
            document.getElementById('modal-food-title').innerText = 'Order: ' + foodName;
            document.getElementById('order-modal').style.display = 'flex';
        }
        function closeModal() {
            document.getElementById('order-modal').style.display = 'none';
        }
        async function submitOrder() {
            let name = document.getElementById('customer-name').value.trim();
            let seat = document.getElementById('seat-number').value.trim();
            let food = document.getElementById('selected-food').value;
            if(!name || !seat) { alert('Please enter both your name and seat number.'); return; }
            closeModal();
            let res = await fetch('/api/order', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({foodName: food, customerName: name, seatNumber: seat})
            });
            if(res.ok) {
                document.getElementById('status-box').style.display = 'block';
                startPolling();
            } else {
                alert('Another order is currently awaiting touch confirmation. Please try again shortly.');
            }
        }
        function startPolling() {
            if(checkInterval) clearInterval(checkInterval);
            checkInterval = setInterval(async () => {
                let res = await fetch('/api/order/status');
                let data = await res.json();
                if(!data.pending) {
                    clearInterval(checkInterval);
                    document.getElementById('status-box').style.background = 'rgba(34, 197, 94, 0.15)';
                    document.getElementById('status-box').style.borderColor = '#22c55e';
                    document.getElementById('status-text').innerText = 'Order Confirmed & Sent to Kitchen!';
                    setTimeout(() => {
                        document.getElementById('status-box').style.display = 'none';
                        document.getElementById('status-box').style.background = 'rgba(14, 165, 233, 0.15)';
                        document.getElementById('status-box').style.borderColor = '#38bdf8';
                    }, 4000);
                }
            }, 1000);
        }
        loadMenu();
    </script>
</body>
</html>
)===";

#endif

