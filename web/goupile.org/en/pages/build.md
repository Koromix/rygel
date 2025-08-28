> [!IMPORTANT]
> The documentation is currently being written and improved; some sections are incomplete.

# Global Structure

*Writing in progress*

# Page Development

## Interface

To create an eCRF, you need to log in to the administrator interface, display the "Projects" configuration panel [1], and then click on the "Access" menu for the project of interest [2]. You can bookmark the opened link to access the project directly later.

<div class="screenshot"><img src="{{ ASSET static/help/dev_access.webp }}" alt=""/></div>

## Default View

A new tab opens in your browser. The black banner at the top of the page allows you to configure the page display. You can show (or hide) one or two panels by selecting (or deselecting) the panel(s) of interest. By default, the "List" panel [2] is selected. The different configuration panels are: "Code" [1], "List" [2], and "Entry" [3]. The central dropdown menu [4] allows you to navigate between the different pages of your eCRF (here the first page is named "Introduction"). The middle dropdown menu [5] allows you to navigate between different sub-projects if your project has been subdivided. Finally, the dropdown menu on the far right [6] allows you to change your password or log out of your session.

<div class="screenshot"><img src="{{ ASSET static/help/dev_panels.webp }}" alt=""/></div>

### Code

The "Code" configuration panel allows you to edit your eCRF. It contains two tabs: "Application" and "Form". By default, the "Form" tab is displayed.

The "Form" tab allows you to edit the content of your eCRF for a given page (here "Introduction". Remember, navigation between the different pages of your form is done via the dropdown menu [1]).
The content is edited in code lines via a script editor. The programming language is JavaScript. Knowledge of the language is not necessary for creating and editing simple scripts. Editing the eCRF and the different code modules will be covered in more detail later.

<div class="screenshot"><img src="{{ ASSET static/help/dev_code1.webp }}" alt=""/></div>

The "Application" tab allows you to edit the general structure of your eCRF. It allows you to define the different pages and sets of pages. The structure is also edited in code lines via a script editor (also Javascript). Editing the structure of the eCRF and the different code modules will be covered in more detail later.

<div class="screenshot"><img src="{{ ASSET static/help/dev_code2.webp }}" alt=""/></div>

### List

The "List" panel allows you to add new observations ("Add a follow-up") and monitor data collection. The "ID" variable corresponds to the identifier of a data collection form. It is a sequential number by default but this can be configured. The variables "Introduction", "Advanced", and "Layout" correspond to the three pages of the example eCRF.

<div class="screenshot"><img src="{{ ASSET static/help/dev_list.webp }}" alt=""/></div>

### Entry

The "Entry" configuration panel allows you to perform the collection of a new observation (new patient) or modify a given observation (after selecting the observation in the "List" configuration panel). Navigation between the different pages of the eCRF can be done with the dropdown menu [1] or the navigation menu [2]. After completing the entry of an observation, click "Save" [3].

<div class="screenshot"><img src="{{ ASSET static/help/dev_entry.webp }}" alt=""/></div>

## Standard Widgets

Widgets are created by calling predefined functions with the following syntax:

<p class="code"><span style="color: #24579d;">function</span> ( <span style="color: #c09500;">"variable_name"</span>, <span style="color: #842d3d;">"Label presented to the user"</span> )</p></p>

Variable names must start with a letter or \_, followed by zero or more alphanumeric characters or \_. They must not contain spaces, accented characters, or any other special characters.

Most widgets accept optional parameters in this manner:

<p class="code"><span style="color: #24579d;">function</span> ( <span style="color: #c09500;">"variable_name"</span>, <span style="color: #842d3d;">"Label presented to the user"</span>, {
    <span style="color: #054e2d;">option1</span> : <span style="color: #431180;">value</span>,
    <span style="color: #054e2d;">option2</span> : <span style="color: #431180;">value</span>
} )</p>

Pay attention to the code syntax. When parentheses or quotes do not match, an error occurs and the displayed page cannot be updated until the error is fixed. The section on errors contains more information on this.

### Information Entry

<div class="table-wrapper colwidths-auto docutils container">
    <table class="docutils align-default">
        <tr>
            <th>Widget</th>
            <th>Code</th>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 58px; object-position: 0px 0px;"/></td>
            <td class="code">form.text("variable", "Label")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 58px; object-position: 0 -53px;"/></td>
            <td class="code">form.number("variable", "Label")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 58px; object-position: 0 -106px;"/></td>
            <td class="code">form.slider("variable", "Label")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 58px; object-position: 0 -175px;"/></td>
            <td class="code">form.date("variable_name", "Label")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 63px; object-position: 0 -228px;"/></td>
            <td class="code">form.binary("variable_name", "Label")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 63px; object-position: 0 -285px;"/></td>
            <td class="code">form.enum("variable_name", "Label", [
    ["modality1", "modality1 Label"],
    ["modality2", "modality2 Label"]
    ])</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 58px; object-position: 0 -340px;"/></td>
            <td class="code">form.enumDrop("variable_name", "Label", [
    ["modality1", "modality1 Label"],
    ["modality2", "modality2 Label"]
    ])</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 74px; object-position: 0 -392px;"/></td>
            <td class="code">form.enumRadio("variable_name", "Label", [
    ["modality1", "modality1 Label"],
    ["modality2", "modality2 Label"]
    ])</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 63px; object-position: 0 -461px;"/></td>
            <td class="code">form.multi("variable_name", "Label", [
    ["modality1", "modality1 Label"],
    ["modality2", "modality2 Label"]
    ])</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 74px; object-position: 0 -517px;"/></td>
            <td class="code">form.multiCheck("variable_name", "Label", [
    ["modality1", "modality1 Label"],
    ["modality2", "modality2 Label"]
    ])</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 51px; object-position: 0 -586px;"/></td>
            <td class="code">form.calc("variable_name", 18)</td>
        </tr>
    </table>
</div>

### Other Widgets

<div class="table-wrapper colwidths-auto docutils container">
    <table class="docutils align-default">
        <tr>
            <th>Widget</th>
            <th>Code</th>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 152px; object-position: 0 -632px;"/></td>
            <td class="code">form.section("Section Name", () => {
    form.text("variable1", "Label 1")
    form.number("variable2", "Label 2")
    })</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 27px; object-position: 0 -779px;"/></td>
            <td class="code">form.output("This is &lt;b&gt;text&lt;/b&gt; content")</td>
        </tr>
        <tr>
            <td class="widget"><img src="{{ ASSET static/help/widgets.png }}" style="height: 27px; object-position: 0 -801px;"/></td>
            <td class="code">form.output(html`This is &lt;b&gt;HTML&lt;/b&gt; content`)</td>
        </tr>
    </table>
</div>

<style>
    .widget {
        width: 100px;
        padding: 0.1em;
        background: #efefef;
    }
    .widget > img {
        width: 237px;
        object-fit: cover;
    }
    .code {
        font-family: monospace;
        white-space: pre-wrap;
        font-size: 1.1em;
        margin-left: 2em;
        color: #444;
    }
</style>

# Images and Other Files

It is possible to integrate images, videos, and all types of files, directly hosted by Goupile. To do this, open the publication panel (accessible above the editor), then click on the "Add a file" link [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/dev_file1.webp }}" style="height: 240px;" alt=""/>
    <img src="{{ ASSET static/help/dev_file2.webp }}" style="height: 240px;" alt=""/>
</div>

You can then select a file to add from your computer, and rename it if you wish. You can also place it in a hierarchy by giving it a name like "images/alps/montblanc.png".

Once the image is added, you can directly display it in your pages using the `form.output` widget and the following HTML code:

```js
form.output(html`
    <img src=${ENV.urls.files + 'apple.png'} alt="Apple Logo" />
`)
```

Using `ENV.urls.files` to build the URL ensures a URL that will change with each project publication, to avoid problems related to browser caching. However, each file is also accessible via `/projet/files/images/alpes/montagne.png`, and this URL is stable from outside Goupile.

> [!NOTE]
> The filename `favicon.png` is special. If you place an image named favicon.png it will replace the favicon displayed in the browser, and the icon displayed on the login screen.

# Unique Identifiers

Each record in Goupile has two unique identifiers:

- The ULID identifier, which is an internally generated random identifier.
- The HID identifier (for *Human ID*), which must be provided by your script.

For example, here we assign the value of the `num_inclusion` field to the HID. This way, this inclusion number will be visible as an identifier in the monitoring table.

If you want the ID to be another field, this is accessible via the ```meta.hid``` variable.

```js
form.number("num_inclusion", "Inclusion Number")
meta.hid = values.num_inclusion
```

Each record also has an auto-incremental `sequence` number, an internal auto-incremental sequence for the form.

# Errors and Controls

## Error Handling

Min and max present on the form.number widget are simply convenient shortcuts.

Take the example of the text widget: ```form.text("*num_inclusion", "Inclusion N°")```

This is a JavaScript string. Therefore, we can call all the methods present here: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String

```js
if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Incorrect format (5 digits expected)")
```

or

```js
if (values.inclusion_num && !values.inclusion_num.match(/^(REN|GRE|CFD|LRB|STE)[A-Z][a-z][A-Z]$/))
    form.error("inclusion_num", html`Incorrect format.<br/> Enter the center code (REN, GRE, CFD, LRB, STE) then the first 2 letters of the Last Name and the first letter of the First Name.<br/>Example: Mr Philippe DURAND in Rennes: 'RENDuP'`)
```

Just always remember to check that the string is not empty (`null`).

If you want the error to be delayed until the page is validated, you need to add `true` as the third parameter of `form.error()`.

```js
// The true at the end of form.error() indicates that the error is delayed until validation
if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Incorrect format (5 digits expected)", true)
```

To test regular expressions live: https://regex101.com/

## Consistency Checks

*Writing in progress*

# Project Publication

Once your form is ready, you must publish it to make it accessible to other users. After publication, users will be able to enter data into these forms.

To do this, click the Publish button at the top right of the code editing panel. This will display the publication panel (visible in the screenshot on the left).

<div class="screenshot"><img src="{{ ASSET static/help/publish.webp }}" alt=""/></div>

This panel summarizes the changes made and the actions that the publication will entail. In the screenshot on the right, we see that a page has been modified locally (named "accueils") and will be made public after the changes are accepted.
