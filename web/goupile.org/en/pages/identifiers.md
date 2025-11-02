# TID and sequence

Each record in Goupile has two unique identifiers:

- A **TID identifier**, which is a 26-character string encoding the time of the record and a random part (e.g., *01K5EY3SCEM1D1NBABXXDZW7XP*).
- A **sequence identifier**, which is an incremented number for each record, starting at 1.

Use the TID to join different tables in exported data. The TID value is available in the `__tid` column of each exported table.

# Custom counters

## Incremental counters

You can create custom counters that are **incremented for each record** (starting at 1) based on conditions defined in the code.

The `meta.count(key, secret = false)` function is used to create a counter.

The example below shows how to divide patients into 3 groups. After each record, one of the three counters below is incremented and assigned to the participant:

- *smoking_active*: counts the number of active smokers
- *smoking_stopped*: counts the number of former smokers
- *smoking_no*: counts the number of non-smokers

```js
form.enum("smoking", "Smoking", [
    ["active", "Active smoker"],
    ["stopped", "Former smoker"],
    ["non", "Non-smoker"]
])

if (values.smoking)
    meta.count("smoking_" + values.smoking)
```

After including several participants, the counter value for each participant is available in the export under the `@counters` tab of the XLSX file. The screenshot below shows 3 participants: the first is a former smoker, the next two are active smokers.

<div class="screenshot"><img src="{{ ASSET static/help/dev/count.webp }}" height="200" alt=""/></div>

## Randomization

Goupile's randomization system is an **extension of the counters** introduced above.

Use the `meta.randomize(key, max, secret = false)` function to assign a randomized counter. The randomized counter is not sequential; each participant receives a random value between 1 and *max*, in blocks of size *max*.
Once *max* participants are included, the counter resets for the next block.

For example, setting a maximum of 4 ensures that the first 4 participants receive values 1, 2, 3, and 4 in random order. The next 4 participants will do the same:

```js
meta.randomize("group", 4)
```

As in the sequential counter example, you can *stratify randomization* by using a different key, depending on a variable in the form.

The example below performs randomization stratified by gender—participants receive a randomization number depending on their gender:

- *groupe_M*: male randomization
- *groupe_F*: female randomization
- *groupe_NB*: non-binary randomization

```js
form.enum("gender", "Gender", [
    ["M", "Male"],
    ["F", "Female"],
    ["NB", "Non-binary"]
])

if (values.gender)
    meta.count("group_" + values.gender)
```

The value of each randomized counter is available in the `@counters` tab of the export file. The screenshot below shows group assignments for 8 inclusions using `meta.randomize("group", 4)`.

<div class="screenshot"><img src="{{ ASSET static/help/dev/randomize.webp }}" height="200" alt=""/></div>

By default, counters are not secret, meaning they can be read from another form page and displayed or used to adapt subsequent pages.

To define a secret counter that is only available in the final export file, set the third parameter `secret` of the randomization function to *true*:

```js
// Secret randomization
meta.randomize("group", 4, true)
```

# Summary

For each form page, you can define an additional identifier called *summary*, which is displayed instead of the date in the data monitoring table of records (see screenshot below).

To do so, assign a value to `meta.summary` in the form script. In the example below, the value displayed in the "Introduction" column is built based on the age entered in the corresponding variable:

```js
form.number("*age", "Age", {
    min: 0,
    max: 120
})

if (values.age != null)
    meta.summary = values.age + (values.age > 1 ? ' years' : ' year')
```

<div class="screenshot"><img src="{{ ASSET static/help/dev/summary.webp }}" height="200" alt=""/></div>

You can use this feature to display a manually created inclusion number (for instance, if the inclusion number is generated outside Goupile), as shown below:

```js
form.number("inclusion_num", "Inclusion number")
meta.summary = values.inclusion_num
```
