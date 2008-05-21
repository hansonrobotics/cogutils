/**
 * FrameQuery.cc
 *
 * Implement pattern matching for RelEx semantic frame queries.
 * The pattern matching is performed at the frame level, 
 * and thus should provide at once a looser but more 
 * semnatically correct matching.
 *
 * Works, at least for basic queries.
 *
 * XXX todo-- should have is_query look for 
 *   <EvaluationLink>
 *       <Element class="DefinedFrameElementNode" name="#Questioning:Message"/>
 *       <ListLink>
 *          <Element class="DefinedLinguisticConceptNode" name="#what"/>
 *          <Element class="ConceptNode" name="_$qVar_be8ca85e"/>
 *       </ListLink>
 *  </EvaluationLink>
 *  That is, a query is recognized if something like the above is in
 *  it.
 *
 * Copyright (c) 2008 Linas Vepstas <linas@linas.org>
 */

#include <stdio.h>

#include "Node.h"
#include "FrameQuery.h"

using namespace opencog;


FrameQuery::FrameQuery(void)
{
}

FrameQuery::~FrameQuery()
{
}

#ifdef DEBUG
static void prt(Atom *atom)
{
   std::string str = atom->toString();
   printf ("%s\n", str.c_str());
}
#endif

/* ======================================================== */
/* Routines to help put the query into normal form. */

/**
 * Return true, if the node is, for example, _subj or _obj
 */
bool FrameQuery::is_frame_elt(Atom *atom)
{
	if (DEFINED_FRAME_ELEMENT_NODE == atom->getType()) return true;
	return false;
}

bool FrameQuery::discard_question_markup(Atom *atom)
{
	Node *n = dynamic_cast<Node *>(atom);
	if(!n) return false;

	Type atype = atom->getType();
	if (DEFINED_LINGUISTIC_CONCEPT_NODE == atype)
	{
		const char *name = n->getName().c_str();

		/* Throw away #what, #which, etc. 
		 * as that frame will never occur as a part of the answer.
		 */
		if (!strcmp("#hyp", name)) do_discard = true;
		if (!strcmp("#what", name)) do_discard = true;
		if (!strcmp("#which", name)) do_discard = true;
		if (!strcmp("#when", name)) do_discard = true;
		if (!strcmp("#where", name)) do_discard = true;
		if (!strcmp("#why", name)) do_discard = true;
		if (!strcmp("#how", name)) do_discard = true;
		if (!strcmp("#truth-query", name)) do_discard = true;
	}

	return false;
}

/**
 * Discard all simple relex relations, keeping only 
 * frame relations.
 *
 * Return true to discard.
 */
bool FrameQuery::discard_eval_markup(Atom *atom)
{
	Type atype = atom->getType();

	if (LIST_LINK == atype)
	{
		Handle ah = TLB::getHandle(atom);
		foreach_outgoing_atom(ah, &FrameQuery::discard_question_markup, this);
		return false;
	}

	/* Explicitly discard standard relex relations */
	if (DEFINED_LINGUISTIC_RELATIONSHIP_NODE == atype)
	{
		do_discard = true;
		return false;
	}
	if (DEFINED_FRAME_ELEMENT_NODE != atype) return false;

	Node *n = dynamic_cast<Node *>(atom);
	if(!n) return false;

	/* By default, keep frame elt links */
	do_discard = false;

	const char *name = n->getName().c_str();

	/* Throw away #Questioning,
	 * as that frame will never occur as a part of the answer.
	 */
	if (!strcmp("#Questioning:Message", name)) do_discard = true;

	return false;
}

bool FrameQuery::discard_heir_markup(Atom *atom)
{

	Node *n = dynamic_cast<Node *>(atom);
	if(!n) return false;
	Type atype = atom->getType();

	const char *name = n->getName().c_str();
	if (CONCEPT_NODE == atype)
	{
		/* frame links never have concept nodes in them. */
		do_discard = true;
		return false;
	}

	if (DEFINED_FRAME_NODE == atype)
	{
		/* Throw away #Questioning,
		 * as that frame will never occur as a part of the answer.
		 */
		if (!strcmp("#Questioning", name)) do_discard = true;
	}

	else if (DEFINED_LINGUISTIC_CONCEPT_NODE == atype)
	{
		/* Throw away #what, #which, etc. 
		 * as that frame will never occur as a part of the answer.
		 */
		if (!strcmp("#hyp", name)) do_discard = true;
		if (!strcmp("#what", name)) do_discard = true;
		if (!strcmp("#which", name)) do_discard = true;
		if (!strcmp("#when", name)) do_discard = true;
		if (!strcmp("#where", name)) do_discard = true;
		if (!strcmp("#why", name)) do_discard = true;
		if (!strcmp("#how", name)) do_discard = true;
		if (!strcmp("#truth-query", name)) do_discard = true;
	}

	return false;
}

/**
 * This method is called once for every top-level link
 * in the candidate query graph.  Out if this, it picks
 * out those relationships that should form a part of a query.
 *
 * For frame-based queries, we try to match up the frame
 * parts of the graph; using the current variant of the
 * frame-to-opencog mapping.
 */
bool FrameQuery::assemble_predicate(Atom *atom)
{
	Handle ah = TLB::getHandle(atom);
	Type atype = atom->getType();
	if (EVALUATION_LINK == atype)
	{
		bool keep = foreach_outgoing_atom(ah, &FrameQuery::is_frame_elt, this);
		if (!keep) return false;

		do_discard = true;
		foreach_outgoing_atom(ah, &FrameQuery::discard_eval_markup, this);
		if (do_discard) return false;
	}
	else if (INHERITANCE_LINK == atype)
	{
		/* Discard structures that won't appear in the answer,
		 * or that we don't want to match to.  */
		do_discard = false;
		foreach_outgoing_atom(ah, &FrameQuery::discard_heir_markup, this);
		if (do_discard) return false;
	}
	else
	{
		return false;
	}

	// Its a keeper, add this to our list of acceptable predicate terms.
	add_to_predicate(ah);

	return false;
}

/* ======================================================== */
/* rutime matching routines */

/**
 * Are two nodes "equivalent", as far as the opencog representation 
 * of RelEx expressions are concerned? 
 *
 * Return true to signify a mismatch,
 * Return false to signify equivalence.
 */
bool FrameQuery::node_match(Atom *aa, Atom *ab)
{
	// If we are here, then we are comparing nodes.
	// The result of comparing nodes depends on the
	// node types.
	Type ntype = aa->getType();

	// DefinedLinguisticConcept nodes must match exactly;
	// so if we are here, there's already a mismatch.
	if (DEFINED_LINGUISTIC_CONCEPT_NODE == ntype) return true;
	if (DEFINED_FRAME_NODE == ntype) return true;
	if (DEFINED_FRAME_ELEMENT_NODE == ntype) return true;

	// Concept nodes can match if they inherit from the same concept.
	if (CONCEPT_NODE == ntype)
	{
		bool mismatch = concept_match(aa, ab);
		// printf("tree_comp concept mismatch=%d\n", mismatch);
		return mismatch;
	}

	fprintf(stderr, "Error: unexpected node type %d %s\n", ntype,
	        ClassServer::getTypeName(ntype));

	std::string sa = aa->toString();
	std::string sb = ab->toString();
	fprintf (stderr, "unexpected comp %s\n"
	                 "             to %s\n", sa.c_str(), sb.c_str());

	return true;
}

/* ===================== END OF FILE ===================== */
