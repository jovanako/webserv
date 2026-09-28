document.addEventListener('DOMContentLoaded', function () {
    const btn = document.getElementById('test-btn');
    const msg = document.getElementById('msg');

    if (btn && msg) {
        btn.addEventListener('click', function () {
            fetch('/data.json')
                .then(response => response.json())
                .then(data => {
                    msg.textContent = `Server responded: "${data.message}" (Version: ${data.version})`;
                })
                .catch(err => {
                    msg.textContent = 'Error loading JSON: ' + err;
                });
        });
    }
});