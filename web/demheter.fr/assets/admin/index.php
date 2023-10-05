<!DOCTYPE html>
<html>
    <head>
        <title>News DEMHETER</title>

        <meta charset="utf-8">
        <link rel="stylesheet" href="assets/admin.css">

        <script>
            function login(e) {
                console.log(e);
                e.preventDefault();

                let target = e.target;
                let password = target.elements.password.value;

                fetch('api.php?method=login', {
                    method: 'POST',
                    body: JSON.stringify({
                        password: password
                    })
                }).then(response => {
                    if (response.ok)
                        window.location.href = "news.php";
                });
            }
        </script>
    </head>

    <body>
        <div class="page">
            <div class="dialog screen">
                <form onsubmit="login(event)">
                    <div class="title">Back-office DEMHETER</div>
                    <div class="main">
                        <label>
                            <span>Mot de passe</span>
                            <input name="password" type="password" />
                        </label>
                    </div>

                    <div class="footer">
                        <button>Valider</button>
                    </div>
                </form>
            </div>
        </div>
    </body>
</html>
