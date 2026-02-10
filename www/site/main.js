function calculateClientSide() {
    const weight = parseFloat(document.getElementById('weight').value);
    const planet = document.getElementById('planet').value;
    let multiplier = 1.0;
    let planetName = "";

    if (isNaN(weight)) {
        document.getElementById('result').innerHTML = "<span style='color:red'>Please enter a valid weight.</span>";
        return;
    }

    switch(planet) {
        case 'mars':
            multiplier = 0.38;
            planetName = "Mars";
            break;
        case 'jupiter':
            multiplier = 2.34;
            planetName = "Jupiter";
            break;
        case 'moon':
            multiplier = 0.16;
            planetName = "The Moon";
            break;
    }

    const finalWeight = (weight * multiplier).toFixed(2);
    let result = document.getElementById('result');
    result.innerHTML = `<strong>Result:</strong> A ${weight}kg object weighs <strong>${finalWeight}kg</strong> on ${planetName}.`;
    result.style.display = 'block';
}
