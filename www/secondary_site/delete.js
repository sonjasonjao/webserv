let filedelete = document.querySelector("#filedelete");
filedelete.addEventListener('submit', (e) => {
	e.preventDefault();
	let data = new FormData(filedelete);
	const fileName = data.get("delfile");
		fetch(fileName, {
			method: "DELETE"
		})
		.then(response => {
			if (response.status !== 204)
				return response.text();
			return "<!DOCTYPE html>\
				<html lang=\"en\">\
					<head>\
						<meta charset=\"UTF-8\">\
						<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
						<title>204 No Content</title>\
						<link rel=\"stylesheet\" href=\"/style.css\">\
					</head>\
					<body>\
						<div class=\"page-wrapper\">\
							<h1>204: No Content</h1>\
							<p>Success! File was deleted.</p>\
							<button type=\"button\" class=\"link\" onclick=\"location.href='/index.html'\">Back to main page</button>\
							</div>\
						<p class=\"footer\">\
							© Johnny, Sonja, and Thiwanka 2026. Built for the 42 project Webserv.\
						</p>\
					</body>\
				</html>";
		})
		.then(html => {
				document.open();
				document.write(html);
				document.close();
		});
});
