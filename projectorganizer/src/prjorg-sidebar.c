/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
#include <gtkcompat.h>

#include "prjorg-utils.h"
#include "prjorg-project.h"
#include "prjorg-sidebar.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

enum
{
	FILEVIEW_COLUMN_ICON,
	FILEVIEW_COLUMN_NAME,
	FILEVIEW_COLUMN_COLOR,
	FILEVIEW_N_COLUMNS,
};

typedef enum
{
	MATCH_FULL,
	MATCH_PREFIX,
	MATCH_PATTERN
} MatchType;

static GdkColor external_color;

static GtkWidget *s_file_view_vbox = NULL;
static GtkWidget *s_file_view = NULL;
static GtkTreeStore *s_file_store = NULL;
static gboolean s_follow_editor = TRUE;

static struct
{
	GtkWidget *expand;
	GtkWidget *collapse;
	GtkWidget *follow;
	GtkWidget *add;
} s_project_toolbar = {NULL, NULL, NULL, NULL};


static struct
{
	GtkWidget *widget;

	GtkWidget *dir_label;
	GtkWidget *combo;
	GtkWidget *case_sensitive;
	GtkWidget *full_path;
} s_fif_dialog = {NULL, NULL, NULL, NULL, NULL};


static struct
{
	GtkWidget *widget;

	GtkWidget *dir_label;
	GtkWidget *combo;
	GtkWidget *combo_match;
	GtkWidget *case_sensitive;
	GtkWidget *declaration;
} s_ft_dialog = {NULL, NULL, NULL, NULL, NULL, NULL};


static struct
{
	GtkWidget *widget;

	GtkWidget *find_in_directory;
	GtkWidget *find_file;
	GtkWidget *find_tag;
	GtkWidget *expand;
	GtkWidget *remove_external_dir;
} s_popup_menu;


static gint show_dialog_find_file(gchar *path, gchar **pattern, gboolean *case_sensitive, gboolean *full_path)
{
	gint res;
	GtkWidget *entry;
	gchar *selection;
	GtkSizeGroup *size_group;

	if (!s_fif_dialog.widget)
	{
		GtkWidget *label, *vbox, *ebox;

		s_fif_dialog.widget = gtk_dialog_new_with_buttons(
			_("Find File"), GTK_WINDOW(geany->main_widgets->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
		gtk_dialog_add_button(GTK_DIALOG(s_fif_dialog.widget), "gtk-find", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(s_fif_dialog.widget), GTK_RESPONSE_ACCEPT);

		vbox = ui_dialog_vbox_new(GTK_DIALOG(s_fif_dialog.widget));
		gtk_box_set_spacing(GTK_BOX(vbox), 6);

		size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

		label = gtk_label_new(_("Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(size_group, label);
		s_fif_dialog.combo = gtk_combo_box_text_new_with_entry();
		entry = gtk_bin_get_child(GTK_BIN(s_fif_dialog.combo));
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		ui_entry_add_clear_icon(GTK_ENTRY(entry));
		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

		ebox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ebox), s_fif_dialog.combo, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

		label = gtk_label_new(_("Search inside:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(size_group, label);
		s_fif_dialog.dir_label = gtk_label_new("");
		gtk_misc_set_alignment(GTK_MISC(s_fif_dialog.dir_label), 0, 0.5);

		ebox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(ebox), s_fif_dialog.dir_label, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

		s_fif_dialog.case_sensitive = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
		gtk_button_set_focus_on_click(GTK_BUTTON(s_fif_dialog.case_sensitive), FALSE);

		s_fif_dialog.full_path = gtk_check_button_new_with_mnemonic(_("Search in full path"));
		gtk_button_set_focus_on_click(GTK_BUTTON(s_fif_dialog.full_path), FALSE);

		gtk_box_pack_start(GTK_BOX(vbox), s_fif_dialog.case_sensitive, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), s_fif_dialog.full_path, TRUE, FALSE, 0);
		gtk_widget_show_all(vbox);
	}

	if (path)
		gtk_label_set_text(GTK_LABEL(s_fif_dialog.dir_label), path);
	else
		gtk_label_set_text(GTK_LABEL(s_fif_dialog.dir_label), _("project or external directory"));
	entry = gtk_bin_get_child(GTK_BIN(s_fif_dialog.combo));
	selection = get_selection();
	if (selection)
		gtk_entry_set_text(GTK_ENTRY(entry), selection);
	g_free(selection);
	gtk_widget_grab_focus(entry);

	res = gtk_dialog_run(GTK_DIALOG(s_fif_dialog.widget));

	if (res == GTK_RESPONSE_ACCEPT)
	{
		const gchar *str;

		str = gtk_entry_get_text(GTK_ENTRY(entry));
		*pattern = g_strconcat("*", str, "*", NULL);
		*case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_fif_dialog.case_sensitive));
		*full_path = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_fif_dialog.full_path));
		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(s_fif_dialog.combo), str, 0);
	}

	gtk_widget_hide(s_fif_dialog.widget);

	return res;
}


static gboolean topmost_selected(GtkTreeModel *model, GtkTreeIter *iter, gboolean first)
{
	GtkTreePath *path, *first_path;
	gboolean ret, is_first;
	
	first_path = gtk_tree_path_new_first();
	path = gtk_tree_model_get_path(model, iter);
	
	is_first = gtk_tree_path_compare(first_path, path) == 0;
	ret = gtk_tree_path_get_depth(path) == 1 && ((is_first && first) || (!is_first && !first));
	gtk_tree_path_free(first_path);
	gtk_tree_path_free(path);
	return ret;
}


static gchar *build_path(GtkTreeIter *iter)
{
	GtkTreeIter node;
	GtkTreeIter parent;
	gchar *path = NULL;
	GtkTreeModel *model;
	gchar *name;

	if (!iter)
		return g_strdup(geany_data->app->project->base_path);

	node = *iter;
	model = GTK_TREE_MODEL(s_file_store);

	while (gtk_tree_model_iter_parent(model, &parent, &node))
	{
		gtk_tree_model_get(model, &node, FILEVIEW_COLUMN_NAME, &name, -1);
		if (path == NULL)
			path = g_strdup(name);
		else
			SETPTR(path, g_build_filename(name, path, NULL));
		g_free(name);

		node = parent;
	}

	if (topmost_selected(model, &node, TRUE))
		SETPTR(path, g_build_filename(geany_data->app->project->base_path, path, NULL));
	else
	{
		gtk_tree_model_get(model, &node, FILEVIEW_COLUMN_NAME, &name, -1);
		SETPTR(path, g_build_filename(name, path, NULL));
		g_free(name);
	}
	return path;
}


static void on_expand_all(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(s_file_view));
}


static void collapse(void)
{
	GtkTreeIter iter;
	GtkTreePath *tree_path;
	GtkTreeModel *model = GTK_TREE_MODEL(s_file_store);
	
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(s_file_view));
	
	/* expand the project folder */
	gtk_tree_model_iter_children(model, &iter, NULL);
	tree_path = gtk_tree_model_get_path (model, &iter);
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(s_file_view), tree_path);
	gtk_tree_path_free(tree_path);
}


static void on_collapse_all(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	collapse();
}


static void on_follow_active(GtkToggleToolButton *button, G_GNUC_UNUSED gpointer user_data)
{
	s_follow_editor = gtk_toggle_tool_button_get_active(button);
	gprj_sidebar_update(FALSE);
}


static void on_add_external(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Add External Directory"),
		GTK_WINDOW(geany->main_widgets->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("Add"), GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		
		gprj_project_add_external_dir(filename);
		gprj_sidebar_update(TRUE);
		project_write_config();
		
		g_free (filename);
	}

	gtk_widget_destroy(dialog);
}


static void on_remove_external_dir(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	gchar *name;
	
	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));
	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;

	if (gtk_tree_model_iter_parent(model, &parent, &iter))
		return;
		
	gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &name, -1);
	gprj_project_remove_external_dir(name);
	gprj_sidebar_update(TRUE);
	project_write_config();
	
	g_free(name);
}


static void find_file_recursive(GtkTreeIter *iter, gboolean case_sensitive, gboolean full_path, GPatternSpec *pattern)
{
	GtkTreeModel *model = GTK_TREE_MODEL(s_file_store);
	GtkTreeIter child;
	gboolean iterate;

	iterate = gtk_tree_model_iter_children(model, &child, iter);
	if (iterate)
	{
		while (iterate)
		{
			find_file_recursive(&child, case_sensitive, full_path, pattern);
			iterate = gtk_tree_model_iter_next(model, &child);
		}
	}
	else
	{
		gchar *name;

		if (iter == NULL)
			return;

		gtk_tree_model_get(GTK_TREE_MODEL(model), iter, FILEVIEW_COLUMN_NAME, &name, -1);

		if (full_path)
		{
			gchar *path;

			path = build_path(iter);
			name = get_file_relative_path(geany_data->app->project->base_path, path);
			g_free(path);
		}
		else
			name = g_strdup(name);

		if (!case_sensitive)
			SETPTR(name, g_utf8_strdown(name, -1));

		if (g_pattern_match_string(pattern, name))
		{
			gchar *path, *rel_path;

			path = build_path(iter);
			rel_path = get_file_relative_path(geany_data->app->project->base_path, path);
			msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", rel_path ? rel_path : path);
			g_free(path);
			g_free(rel_path);
		}

		g_free(name);
	}
}


static void find_file(GtkTreeIter *iter)
{
	gchar *pattern_str = NULL;
	gboolean case_sensitive, full_path;
	gchar *path;

	path = build_path(iter);

	if (show_dialog_find_file(iter ? path : NULL, &pattern_str, &case_sensitive, &full_path) == GTK_RESPONSE_ACCEPT)
	{
		GPatternSpec *pattern;

		if (!case_sensitive)
			SETPTR(pattern_str, g_utf8_strdown(pattern_str, -1));

		pattern = g_pattern_spec_new(pattern_str);

		msgwin_clear_tab(MSG_MESSAGE);
		msgwin_set_messages_dir(geany_data->app->project->base_path);
		find_file_recursive(iter, case_sensitive, full_path, pattern);
		msgwin_switch_tab(MSG_MESSAGE, TRUE);
	}

	g_free(pattern_str);
	g_free(path);
}


static void create_dialog_find_tag(void)
{
	GtkWidget *label, *vbox, *ebox, *entry;
	GtkSizeGroup *size_group;

	if (s_ft_dialog.widget)
		return;

	s_ft_dialog.widget = gtk_dialog_new_with_buttons(
		_("Find Tag"), GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_dialog_add_button(GTK_DIALOG(s_ft_dialog.widget), "gtk-find", GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(s_ft_dialog.widget), GTK_RESPONSE_ACCEPT);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(s_ft_dialog.widget));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);

	s_ft_dialog.combo = gtk_combo_box_text_new_with_entry();
	entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));

	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Match type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);

	s_ft_dialog.combo_match = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), "exact");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), "prefix");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), "pattern");
	gtk_combo_box_set_active(GTK_COMBO_BOX(s_ft_dialog.combo_match), 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), s_ft_dialog.combo_match);

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo_match, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Search inside:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);
	s_ft_dialog.dir_label = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(s_ft_dialog.dir_label), 0, 0.5);

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.dir_label, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	s_ft_dialog.case_sensitive = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.case_sensitive), FALSE);

	s_ft_dialog.declaration = gtk_check_button_new_with_mnemonic(_("_Declaration"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.declaration), FALSE);

	g_object_unref(G_OBJECT(size_group));   /* auto destroy the size group */

	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.case_sensitive);
	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.declaration);
	gtk_widget_show_all(vbox);
}


static const char *tm_tag_type_name(const TMTag *tag)
{
	g_return_val_if_fail(tag, NULL);
	switch(tag->type)
	{
		case tm_tag_class_t: return "class";
		case tm_tag_enum_t: return "enum";
		case tm_tag_enumerator_t: return "enumval";
		case tm_tag_field_t: return "field";
		case tm_tag_function_t: return "function";
		case tm_tag_interface_t: return "interface";
		case tm_tag_member_t: return "member";
		case tm_tag_method_t: return "method";
		case tm_tag_namespace_t: return "namespace";
		case tm_tag_package_t: return "package";
		case tm_tag_prototype_t: return "prototype";
		case tm_tag_struct_t: return "struct";
		case tm_tag_typedef_t: return "typedef";
		case tm_tag_union_t: return "union";
		case tm_tag_variable_t: return "variable";
		case tm_tag_externvar_t: return "extern";
		case tm_tag_macro_t: return "define";
		case tm_tag_macro_with_arg_t: return "macro";
		default: return NULL;
	}
	return NULL;
}


static gboolean match(TMTag *tag, const gchar *name, gboolean declaration, gboolean case_sensitive, 
	MatchType match_type, GPatternSpec *pspec, gchar *path)
{
	const gint forward_types = tm_tag_prototype_t | tm_tag_externvar_t;
	gboolean matches = FALSE;
	gint type;
	
	type = declaration ? forward_types : tm_tag_max_t - forward_types;
	matches = tag->type & type;
	
	if (matches)
	{
		gchar *name_case;
		
		if (case_sensitive)
			name_case = g_strdup(tag->name);
		else
			name_case = g_utf8_strdown(tag->name, -1);

		switch (match_type)
		{
			case MATCH_FULL: 
				matches = g_strcmp0(name_case, name) == 0;
				break;
			case MATCH_PATTERN:
				matches = g_pattern_match_string(pspec, name_case);
				break;
			case MATCH_PREFIX:
				matches = g_str_has_prefix(name_case, name);
				break;
		}
		g_free(name_case);
	}
		
	if (matches && path)
	{
		gchar *relpath;
		
		relpath = get_file_relative_path(path, tag->file->file_name);
		matches = relpath && !g_str_has_prefix(relpath, "..");
		g_free(relpath);
	}
	
	return matches;
}


static void find_tags(const gchar *name, gboolean declaration, gboolean case_sensitive, MatchType match_type, gchar *path)
{
	GPtrArray *tags_array = geany_data->app->tm_workspace->tags_array;
	guint i;
	gchar *name_case;
	GPatternSpec *pspec;
	
	if (case_sensitive)
		name_case = g_strdup(name);
	else
		name_case = g_utf8_strdown(name, -1);
		
	pspec = g_pattern_spec_new(name_case);

	msgwin_set_messages_dir(geany_data->app->project->base_path);
	msgwin_clear_tab(MSG_MESSAGE);
	for (i = 0; i < tags_array->len; i++) /* TODO: binary search */
	{
		TMTag *tag = tags_array->pdata[i];

		if (match(tag, name_case, declaration, case_sensitive, match_type, pspec, path))
		{
			gchar *scopestr = tag->scope ? g_strconcat(tag->scope, "::", NULL) : g_strdup("");
			gchar *relpath;
			
			relpath = get_file_relative_path(geany_data->app->project->base_path, tag->file->file_name);
			msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%lu:\n\t[%s]\t %s%s%s", relpath,
				tag->line, tm_tag_type_name(tag), scopestr, tag->name, tag->arglist ? tag->arglist : "");
			g_free(scopestr);
			g_free(relpath);
		}
	}
	msgwin_switch_tab(MSG_MESSAGE, TRUE);
	
	g_free(name_case);
	g_free(pspec);
}


static void find_tag(GtkTreeIter *iter)
{
	gchar *selection;
	gchar *path;
	GtkWidget *entry;

	create_dialog_find_tag();

	entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));

	path = build_path(iter);
	if (iter)
		gtk_label_set_text(GTK_LABEL(s_ft_dialog.dir_label), path);
	else
		gtk_label_set_text(GTK_LABEL(s_ft_dialog.dir_label), _("project or external directory"));

	selection = get_selection();
	if (selection)
		gtk_entry_set_text(GTK_ENTRY(entry), selection);
	g_free(selection);

	gtk_widget_grab_focus(entry);

	if (gtk_dialog_run(GTK_DIALOG(s_ft_dialog.widget)) == GTK_RESPONSE_ACCEPT)
	{
		const gchar *name;
		gboolean case_sensitive, declaration;
		MatchType match_type;

		name = gtk_entry_get_text(GTK_ENTRY(entry));
		case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_ft_dialog.case_sensitive));
		declaration = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_ft_dialog.declaration));
		match_type = gtk_combo_box_get_active(GTK_COMBO_BOX(s_ft_dialog.combo_match));

		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo), name, 0);

		find_tags(name, declaration, case_sensitive, match_type, iter?path:NULL);
	}
	
	g_free(path);
	gtk_widget_hide(s_ft_dialog.widget);
}


static void on_find_file(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));
	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;

	if (!gtk_tree_model_iter_has_child(model, &iter))
	{
		if (gtk_tree_model_iter_parent(model, &parent, &iter))
			find_file(&parent);
		else
			find_file(NULL);
	}
	else
		find_file(&iter);
}


static void on_find_tag(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter iter, parent;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));
	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;

	if (!gtk_tree_model_iter_has_child(model, &iter))
	{
		if (gtk_tree_model_iter_parent(model, &parent, &iter))
			find_tag(&parent);
		else
			find_tag(NULL);
	}
	else
		find_tag(&iter);
}


static void on_reload_project(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gprj_project_rescan();
	gprj_sidebar_update(TRUE);
}


static void on_open_clicked(void)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));

	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		if (gtk_tree_model_iter_has_child(model, &iter))
		{
			GtkTreeView *tree_view;
			GtkTreePath *tree_path;

			tree_view = GTK_TREE_VIEW(s_file_view);
			tree_path = gtk_tree_model_get_path (model, &iter);

			if (gtk_tree_view_row_expanded(tree_view, tree_path))
				gtk_tree_view_collapse_row(tree_view, tree_path);
			else
				gtk_tree_view_expand_row(tree_view, tree_path, FALSE);
			gtk_tree_path_free(tree_path);
		}
		else
		{
			gchar *name;
			GIcon *icon;

			gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_ICON, &icon, -1);

			if (!icon)
			{
				/* help string doesn't have icon */
				return;
			}

			name = build_path(&iter);
			open_file(name);
			g_free(name);
			g_object_unref(icon);
		}
	}
}


static gboolean on_button_release(G_GNUC_UNUSED GtkWidget * widget, GdkEventButton * event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 3)
	{
		GtkTreeSelection *treesel;
		GtkTreeModel *model;
		GtkTreeIter iter;

		treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));

		if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
			return FALSE;

		gtk_widget_set_sensitive(s_popup_menu.expand, gtk_tree_model_iter_has_child(model, &iter));
		gtk_widget_set_sensitive(s_popup_menu.remove_external_dir, topmost_selected(model, &iter, FALSE));

		gtk_menu_popup(GTK_MENU(s_popup_menu.widget), NULL, NULL, NULL, NULL,
						event->button, event->time);
		return TRUE;
	}

	return FALSE;
}


static gboolean on_button_press(G_GNUC_UNUSED GtkWidget * widget, GdkEventButton * event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) 
	{
		on_open_clicked();
		return TRUE;
	}

	return FALSE;
}


static gboolean on_key_press(G_GNUC_UNUSED GtkWidget * widget, GdkEventKey * event, G_GNUC_UNUSED gpointer data)
{
	if (event->keyval == GDK_Return
		|| event->keyval == GDK_ISO_Enter
		|| event->keyval == GDK_KP_Enter
		|| event->keyval == GDK_space)
	{
		on_open_clicked();
		return TRUE;
	}
	return FALSE;
}


static void expand_all(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeIter iter;
	GtkTreeModel *model;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));

	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		GtkTreePath *tree_path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(s_file_view), tree_path, TRUE);
		gtk_tree_path_free(tree_path);
	}
}


static void on_find_in_files(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeIter iter, parent;
	GtkTreeModel *model;
	gchar *path;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));

	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;

	if (!gtk_tree_model_iter_has_child(model, &iter))
	{
		if (gtk_tree_model_iter_parent(model, &parent, &iter))
			path = build_path(&parent);
		else
			path = build_path(NULL);
	}
	else
		path = build_path(&iter);

	search_show_find_in_files_dialog(path);
	g_free(path);
}


static void create_branch(gint level, GSList *leaf_list, GtkTreeIter *parent,
	GSList *header_patterns, GSList *source_patterns, gboolean project)
{
	GSList *dir_list = NULL;
	GSList *file_list = NULL;
	GSList *elem;

	foreach_slist (elem, leaf_list)
	{
		gchar **path_arr = elem->data;

		if (path_arr[level+1] != NULL)
			dir_list = g_slist_prepend(dir_list, path_arr);
		else
			file_list = g_slist_prepend(file_list, path_arr);
	}

	if (dir_list)
	{
		GSList *tmp_list = NULL;
		GtkTreeIter iter;
		gchar **path_arr = dir_list->data;
		gchar *last_dir_name;
		GIcon *icon_dir = g_icon_new_for_string("gtk-directory", NULL);

		last_dir_name = path_arr[level];

		foreach_slist (elem, dir_list)
		{
			gboolean dir_changed;

			path_arr = (gchar **) elem->data;
			dir_changed = g_strcmp0(last_dir_name, path_arr[level]) != 0;

			if (dir_changed)
			{
				gtk_tree_store_append(s_file_store, &iter, parent);
				gtk_tree_store_set(s_file_store, &iter,
					FILEVIEW_COLUMN_ICON, icon_dir,
					FILEVIEW_COLUMN_NAME, last_dir_name, -1);
				if (!project)
					gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);
				
				create_branch(level+1, tmp_list, &iter, header_patterns, source_patterns, project);

				g_slist_free(tmp_list);
				tmp_list = NULL;
				last_dir_name = path_arr[level];
			}

			tmp_list = g_slist_prepend(tmp_list, path_arr);
		}

		gtk_tree_store_append(s_file_store, &iter, parent);
		gtk_tree_store_set(s_file_store, &iter,
			FILEVIEW_COLUMN_ICON, icon_dir,
			FILEVIEW_COLUMN_NAME, last_dir_name, -1);
		if (!project)
			gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);

		create_branch(level+1, tmp_list, &iter, header_patterns, source_patterns, project);

		g_slist_free(tmp_list);
		g_slist_free(dir_list);
		g_object_unref(icon_dir);
	}

	foreach_slist (elem, file_list)
	{
		GtkTreeIter iter;
		gchar **path_arr = elem->data;
		GIcon *icon = NULL;
		gchar *content_type = g_content_type_guess(path_arr[level], NULL, 0, NULL);

		if (content_type)
		{
			icon = g_content_type_get_icon(content_type);
			g_free(content_type);
		}

		gtk_tree_store_append(s_file_store, &iter, parent);
		if (patterns_match(header_patterns, path_arr[level]))
		{
			if (! icon)
				icon = g_icon_new_for_string("prjorg-header", NULL);

			gtk_tree_store_set(s_file_store, &iter,
				FILEVIEW_COLUMN_ICON, icon,
				FILEVIEW_COLUMN_NAME, path_arr[level], -1);
			if (!project)
				gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);
		}
		else if (patterns_match(source_patterns, path_arr[level]))
		{
			if (! icon)
				icon = g_icon_new_for_string("prjorg-source", NULL);

			gtk_tree_store_set(s_file_store, &iter,
				FILEVIEW_COLUMN_ICON, icon,
				FILEVIEW_COLUMN_NAME, path_arr[level], -1);
			if (!project)
				gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);
		}
		else
		{
			if (! icon)
				icon = g_icon_new_for_string("prjorg-file", NULL);

			gtk_tree_store_set(s_file_store, &iter,
				FILEVIEW_COLUMN_ICON, icon,
				FILEVIEW_COLUMN_NAME, path_arr[level], -1);
			if (!project)
				gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);
		}

		if (icon)
			g_object_unref(icon);
	}

	g_slist_free(file_list);
}


static void set_intro_message(const gchar *msg)
{
	GtkTreeIter iter;

	gtk_tree_store_append(s_file_store, &iter, NULL);
	gtk_tree_store_set(s_file_store, &iter,
		FILEVIEW_COLUMN_NAME, msg, -1);

	gtk_widget_set_sensitive(s_project_toolbar.expand, FALSE);
	gtk_widget_set_sensitive(s_project_toolbar.collapse, FALSE);
	gtk_widget_set_sensitive(s_project_toolbar.follow, FALSE);
	gtk_widget_set_sensitive(s_project_toolbar.add, FALSE);
}


static void load_project_root(GPrjRoot *root, GtkTreeIter *parent, GSList *header_patterns, GSList *source_patterns, gboolean project)
{
	GSList *lst = NULL;
	GSList *path_list = NULL;
	GSList *elem;
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, root->file_table);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		gchar *path = get_file_relative_path(root->base_dir, key);
		lst = g_slist_prepend(lst, path);
	}
	lst = g_slist_sort(lst, (GCompareFunc) strcmp);

	foreach_slist (elem, lst)
	{
		gchar **path_split;

		path_split = g_strsplit_set(elem->data, "/\\", 0);
		path_list = g_slist_prepend(path_list, path_split);
	}

	if (path_list != NULL)
		create_branch(0, path_list, parent, header_patterns, source_patterns, project);

	if (project)
	{
		if (path_list != NULL)
		{
			gtk_widget_set_sensitive(s_project_toolbar.expand, TRUE);
			gtk_widget_set_sensitive(s_project_toolbar.collapse, TRUE);
			gtk_widget_set_sensitive(s_project_toolbar.follow, TRUE);
			gtk_widget_set_sensitive(s_project_toolbar.add, TRUE);
		}
		else
			set_intro_message(_("Set file patterns under Project->Properties"));
	}
	
	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);
	g_slist_foreach(path_list, (GFunc) g_strfreev, NULL);
	g_slist_free(path_list);
}


static void load_project(void)
{
	GSList *elem, *header_patterns, *source_patterns;
	GtkTreeIter iter;
	gboolean first = TRUE;
	GIcon *icon_dir;
	
	gtk_tree_store_clear(s_file_store);

	if (!g_prj || !geany_data->app->project)
		return;

	icon_dir = g_icon_new_for_string("gtk-directory", NULL);

	header_patterns = get_precompiled_patterns(g_prj->header_patterns);
	source_patterns = get_precompiled_patterns(g_prj->source_patterns);
	
	foreach_slist (elem, g_prj->roots)
	{
		GPrjRoot *root = elem->data;
		gchar *name;
		
		if (first)
			name = g_strconcat("<b>", geany_data->app->project->name, "</b>", NULL);
		else
			name = g_strdup(root->base_dir);

		gtk_tree_store_append(s_file_store, &iter, NULL);
		gtk_tree_store_set(s_file_store, &iter,
			FILEVIEW_COLUMN_ICON, icon_dir,
			FILEVIEW_COLUMN_NAME, name, -1);
		if (!first)
			gtk_tree_store_set(s_file_store, &iter, FILEVIEW_COLUMN_COLOR, &external_color, -1);

		load_project_root(root, &iter, header_patterns, source_patterns, first);

		first = FALSE;
		g_free(name);
	}
	
	collapse();

	g_slist_foreach(header_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(header_patterns);
	g_slist_foreach(source_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(source_patterns);
	g_object_unref(icon_dir);
}


static gboolean find_in_tree(GtkTreeIter *parent, gchar **path_split, gint level, GtkTreeIter *ret)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean iterate;

	model = GTK_TREE_MODEL(s_file_store);

	iterate = gtk_tree_model_iter_children(model, &iter, parent);
	while (iterate)
	{
		gchar *name;
		gint cmpres;

		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &name, -1);

		cmpres = g_strcmp0(name, path_split[level]);
		g_free(name);
		if (cmpres == 0)
		{
			GtkTreeIter foo;
			if (path_split[level+1] == NULL && !gtk_tree_model_iter_children (model, &foo, &iter))
			{
				*ret = iter;
				return TRUE;
			}
			else
				return find_in_tree(&iter, path_split, level + 1, ret);
		}

		iterate = gtk_tree_model_iter_next(model, &iter);
	}

	return FALSE;
}


static gboolean follow_editor_on_idle(gpointer foo)
{
	GtkTreeIter root_iter, found_iter;
	gchar *path = NULL;
	gchar **path_split;
	GeanyDocument *doc;
	GSList *elem;
	GtkTreeModel *model;

	doc = document_get_current();

	if (!doc || !doc->file_name || !geany_data->app->project || !g_prj)
		return FALSE;

	model = GTK_TREE_MODEL(s_file_store);
	gtk_tree_model_iter_children(model, &root_iter, NULL);
	foreach_slist (elem, g_prj->roots)
	{
		GPrjRoot *root = elem->data;
		
		path = get_file_relative_path(root->base_dir, doc->file_name);
		if (path != NULL && !g_str_has_prefix(path, ".."))
			break;
			
		g_free(path);
		path = NULL;
		gtk_tree_model_iter_next(model, &root_iter);
	}
	
	if (!path)
		return FALSE;

	path_split = g_strsplit_set(path, "/\\", 0);

	if (find_in_tree(&root_iter, path_split, 0, &found_iter))
	{
		GtkTreePath *tree_path;
		GtkTreeSelection *treesel;

		tree_path = gtk_tree_model_get_path (model, &found_iter);

		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(s_file_view), tree_path);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(s_file_view), tree_path,
			NULL, FALSE, 0.0, 0.0);
			
		treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));
		gtk_tree_selection_select_iter(treesel, &found_iter);
		gtk_tree_path_free(tree_path);
	}
	
	g_free(path);
	g_strfreev(path_split);
	
	return FALSE;
}


void gprj_sidebar_update(gboolean reload)
{
	if (reload)
		load_project();
	if (s_follow_editor) 
		/* perform on idle - avoids unnecessary jumps on project load */
		plugin_idle_add(geany_plugin, (GSourceFunc)follow_editor_on_idle, NULL);
}


void gprj_sidebar_find_file_in_active(void)
{
	find_file(NULL);
}


void gprj_sidebar_find_tag_in_active(void)
{
	find_tag(NULL);
}


void gprj_sidebar_init(void)
{
	GtkWidget *scrollwin, *toolbar, *item, *image;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	PangoFontDescription *pfd;
	GList *focus_chain = NULL;

	s_file_view_vbox = gtk_vbox_new(FALSE, 0);

	/**** toolbar ****/

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	external_color = gtk_widget_get_style(toolbar)->bg[GTK_STATE_NORMAL];

	item = GTK_WIDGET(gtk_tool_button_new(NULL, NULL));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON(item), "prjorg-refresh");
	ui_widget_set_tooltip_text(item, _("Reload all"));
	g_signal_connect(item, "clicked", G_CALLBACK(on_reload_project), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);

	item = GTK_WIDGET(gtk_separator_tool_item_new());
	gtk_container_add(GTK_CONTAINER(toolbar), item);

	item = GTK_WIDGET(gtk_tool_button_new(NULL, NULL));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON(item), "prjorg-add-external");
	ui_widget_set_tooltip_text(item, _("Add external directory"));
	g_signal_connect(item, "clicked", G_CALLBACK(on_add_external), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	s_project_toolbar.add = item;

	item = GTK_WIDGET(gtk_separator_tool_item_new());
	gtk_container_add(GTK_CONTAINER(toolbar), item);

	item = GTK_WIDGET(gtk_tool_button_new(NULL, NULL));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON(item), "prjorg-expand");
	ui_widget_set_tooltip_text(item, _("Expand all"));
	g_signal_connect(item, "clicked", G_CALLBACK(on_expand_all), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	s_project_toolbar.expand = item;

	item = GTK_WIDGET(gtk_tool_button_new(NULL, NULL));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON(item), "prjorg-collapse");
	ui_widget_set_tooltip_text(item, _("Collapse to project root"));
	g_signal_connect(item, "clicked", G_CALLBACK(on_collapse_all), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	s_project_toolbar.collapse = item;

	item = GTK_WIDGET(gtk_separator_tool_item_new());
	gtk_container_add(GTK_CONTAINER(toolbar), item);

	item = GTK_WIDGET(gtk_toggle_tool_button_new());
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item), TRUE);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON(item), "prjorg-follow");
	ui_widget_set_tooltip_text(item, _("Follow active editor"));
	g_signal_connect(item, "clicked", G_CALLBACK(on_follow_active), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	s_project_toolbar.follow = item;

	gtk_box_pack_start(GTK_BOX(s_file_view_vbox), toolbar, FALSE, FALSE, 0);

	/**** tree view ****/

	s_file_view = gtk_tree_view_new();

	s_file_store = gtk_tree_store_new(FILEVIEW_N_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, GDK_TYPE_COLOR);
	gtk_tree_view_set_model(GTK_TREE_VIEW(s_file_view), GTK_TREE_MODEL(s_file_store));

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "gicon", FILEVIEW_COLUMN_ICON);
	gtk_tree_view_column_add_attribute(column, renderer, "cell-background-gdk", FILEVIEW_COLUMN_COLOR);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "markup", FILEVIEW_COLUMN_NAME);
	gtk_tree_view_column_add_attribute(column, renderer, "cell-background-gdk", FILEVIEW_COLUMN_COLOR);

	gtk_tree_view_append_column(GTK_TREE_VIEW(s_file_view), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(s_file_view), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(s_file_view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(s_file_view), FILEVIEW_COLUMN_NAME);

	pfd = pango_font_description_from_string(geany_data->interface_prefs->tagbar_font);
	gtk_widget_modify_font(s_file_view, pfd);
	pango_font_description_free(pfd);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(s_file_view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(s_file_view), "button-release-event",
			G_CALLBACK(on_button_release), NULL);
/*	row-activated grabs focus for the sidebar, use button-press-event instead */
	g_signal_connect(G_OBJECT(s_file_view), "button-press-event",
			G_CALLBACK(on_button_press), NULL);
	g_signal_connect(G_OBJECT(s_file_view), "key-press-event",
			G_CALLBACK(on_key_press), NULL);

	set_intro_message(_("(Re)open project to start using the plugin"));
	gprj_sidebar_activate(FALSE);

	/**** popup menu ****/

	s_popup_menu.widget = gtk_menu_new();

	image = gtk_image_new_from_icon_name("prjorg-expand", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Expand All"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(expand_all), NULL);
	s_popup_menu.expand = item;

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Find in Files"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_in_files), NULL);
	s_popup_menu.find_in_directory = item;

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Find File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_file), NULL);
	s_popup_menu.find_file = item;

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Find Tag"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_tag), NULL);
	s_popup_menu.find_tag = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Remove External Directory"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_remove_external_dir), NULL);
	s_popup_menu.remove_external_dir = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
					  gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(s_popup_menu.widget), item);
	g_signal_connect_swapped((gpointer) item, "activate",
				 G_CALLBACK(keybindings_send_command),
				 GINT_TO_POINTER(GEANY_KEYS_VIEW_SIDEBAR));

	/**** the rest ****/

	focus_chain = g_list_prepend(focus_chain, s_file_view);
	gtk_container_set_focus_chain(GTK_CONTAINER(s_file_view_vbox), focus_chain);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
					   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), s_file_view);
	gtk_box_pack_start(GTK_BOX(s_file_view_vbox), scrollwin, TRUE, TRUE, 0);

	gtk_widget_show_all(s_file_view_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
				 s_file_view_vbox, gtk_label_new(_("Project")));
}


void gprj_sidebar_activate(gboolean activate)
{
	gtk_widget_set_sensitive(s_file_view_vbox, activate);
}


void gprj_sidebar_cleanup(void)
{
	gtk_widget_destroy(s_file_view_vbox);
}
