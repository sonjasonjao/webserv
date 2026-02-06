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
				<html>\
					<head>\
						<title>204 No Content</title>\
						<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\
						<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\
						<link href=\"https://fonts.googleapis.com/css2?family=Roboto:ital,wght@0,100..900;1,100..900&display=swap\" rel=\"stylesheet\">\
						<style>\
							html {\
								height: 100%;\
							}\
							body {\
								background-color: white;\
								height: 100%;\
							}\
							.wrapper {\
								font-family: \"Roboto\", sans-serif;\
								text-align: center;\
								width: 70%;\
								height: 100%;\
								margin: 0 auto;\
								background-color: #c6ecf7;\
								padding: 15px;\
							}\
						</style>\
					</head>\
					<body>\
						<div class=\"wrapper\">\
							<h1>204: No Content</h1>\
							<p>Success! File was deleted.</p>\
							<a href=\"/index.html\">Back to main page</a>\
						</div>\
					</body>\
				</html>";
		})
		.then(html => {
				document.open();
				document.write(html);
				document.close();
		});
});
