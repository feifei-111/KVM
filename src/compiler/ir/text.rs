pub(crate) fn split_top_level(input: &str, delimiter: char) -> Vec<&str> {
    let mut parts = Vec::new();
    let mut start = 0usize;
    let mut angle = 0usize;
    let mut square = 0usize;
    let mut brace = 0usize;
    let mut paren = 0usize;
    let mut in_string = false;

    for (index, ch) in input.char_indices() {
        if ch == '"' {
            in_string = !in_string;
            continue;
        }
        if in_string {
            continue;
        }
        match ch {
            '<' => angle += 1,
            '>' => angle = angle.saturating_sub(1),
            '[' => square += 1,
            ']' => square = square.saturating_sub(1),
            '{' => brace += 1,
            '}' => brace = brace.saturating_sub(1),
            '(' => paren += 1,
            ')' => paren = paren.saturating_sub(1),
            _ if ch == delimiter
                && angle == 0
                && square == 0
                && brace == 0
                && paren == 0 =>
            {
                parts.push(&input[start..index]);
                start = index + ch.len_utf8();
            }
            _ => {}
        }
    }
    parts.push(&input[start..]);
    parts
}

pub(crate) fn find_top_level_char(input: &str, target: char) -> Option<usize> {
    let mut angle = 0usize;
    let mut square = 0usize;
    let mut brace = 0usize;
    let mut in_string = false;

    for (index, ch) in input.char_indices() {
        if ch == '"' {
            in_string = !in_string;
            continue;
        }
        if in_string {
            continue;
        }
        match ch {
            '<' => angle += 1,
            '>' => angle = angle.saturating_sub(1),
            '[' => square += 1,
            ']' => square = square.saturating_sub(1),
            '{' if angle == 0 && square == 0 && brace == 0 && ch == target => {
                return Some(index);
            }
            '{' => brace += 1,
            '}' => brace = brace.saturating_sub(1),
            _ if ch == target && angle == 0 && square == 0 && brace == 0 => {
                return Some(index);
            }
            _ => {}
        }
    }
    None
}
