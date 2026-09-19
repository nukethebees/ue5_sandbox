namespace CodeFormatTools;

internal sealed class FormatApplication(
    FormatFileSelector file_selector,
    IFileFormatter formatter,
    TextWriter standard_output,
    TextWriter standard_error)
{
    public async Task<int> RunAsync(
        IReadOnlyList<string> arguments,
        string working_directory,
        CancellationToken cancellation_token = default)
    {
        if (!FormatRequest.TryParse(arguments, out var request, out var usage_error))
        {
            standard_error.WriteLine(usage_error);
            standard_error.WriteLine("Usage: CodeFormatTools [--all | --changed | --staged] [--verbose]");
            return 2;
        }

        FileSelection selection;
        try
        {
            selection = await file_selector.SelectAsync(
                request!.Mode,
                working_directory,
                standard_error.WriteLine,
                cancellation_token);
        }
        catch (Exception exception) when (exception is FormatToolException or IOException or UnauthorizedAccessException)
        {
            standard_error.WriteLine($"ERROR: Failed to select files: {exception.Message}");
            return 1;
        }

        if (request.Mode == FormatMode.Staged)
        {
            try
            {
                var conflicts = await file_selector.SelectUnstagedAsync(
                    selection.RepositoryRoot,
                    selection.Files,
                    cancellation_token);
                if (conflicts.Count > 0)
                {
                    standard_error.WriteLine("ERROR: Staged files also have unstaged edits:");
                    foreach (var file_path in conflicts)
                    {
                        standard_error.WriteLine($"  {Path.GetRelativePath(selection.RepositoryRoot, file_path)}");
                    }

                    standard_error.WriteLine("Stage or stash the unstaged edits before running --staged.");
                    return 1;
                }
            }
            catch (FormatToolException exception)
            {
                standard_error.WriteLine($"ERROR: Failed to check staged files: {exception.Message}");
                return 1;
            }
        }

        standard_output.WriteLine($"Running clang-format on {selection.Description}.");
        try
        {
            await formatter.EnsureAvailableAsync(cancellation_token);
        }
        catch (FormatToolException exception)
        {
            standard_error.WriteLine($"ERROR: {exception.Message}");
            return 1;
        }

        var errors = new List<string>();
        foreach (var file_path in selection.Files)
        {
            var relative_path = Path.GetRelativePath(selection.RepositoryRoot, file_path);
            if (request.Verbose)
            {
                standard_output.WriteLine($"Formatting: {relative_path}");
            }

            var result = await formatter.FormatAsync(file_path, cancellation_token);
            if (result.Success)
            {
                continue;
            }

            var error = result.Error ?? "Unknown error";
            errors.Add($"{relative_path}: {error}");
            standard_error.WriteLine($"ERROR formatting {relative_path}: {error}");
        }

        if (errors.Count > 0)
        {
            standard_error.WriteLine();
            standard_error.WriteLine("Errors encountered:");
            foreach (var error in errors)
            {
                standard_error.WriteLine($"  {error}");
            }

            return 1;
        }

        if (request.Mode == FormatMode.Staged)
        {
            try
            {
                await file_selector.StageAsync(selection.RepositoryRoot, selection.Files, cancellation_token);
            }
            catch (FormatToolException exception)
            {
                standard_error.WriteLine($"ERROR: Failed to stage formatted files: {exception.Message}");
                return 1;
            }
        }

        standard_output.WriteLine($"Successfully formatted {selection.Files.Count}/{selection.Files.Count} files.");
        return 0;
    }
}
