let calc = document.querySelector("#number-input");
calc.addEventListener('submit', (e) => {
	e.preventDefault();
	let data = new FormData(calc);
	const a = data.get("number-a");
	const b = data.get("number-b");
		fetch("/cgi-bin/calculator.py?a=" + a + "&b=" + b, {
			method: "GET"
		})
		.then(response => { return response.text(); })
		.then(html => {
				document.open();
				document.write(html);
				document.close();
		});
});
